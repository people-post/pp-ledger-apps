#include "online/Online.h"

#include "net/AdminOperations.h"
#include "common/io/Json.h"
#include "lib/common/BinaryPack.hpp"

#include <chrono>
#include <memory>
#include <thread>

namespace admin::online {

namespace op = pp::operation;

namespace {

int64_t nowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

Online::Online(ui_host::UiApp &app, ui_host::Session &session) : app_(app), session_(session) {
  session_.compare = [this](const op::Operation &operation,
                            std::function<void(std::vector<std::pair<std::string, std::string>>)> done) {
    compare(operation, std::move(done));
  };
  session_.submit = [this](const op::Operation &operation,
                           std::function<void(const ui_host::Session::SubmitProgress &)> report) {
    submit(operation, std::move(report));
  };
}

void Online::compare(const op::Operation &operation,
                     std::function<void(std::vector<std::pair<std::string, std::string>>)> done) {
  auto lines = std::make_shared<std::vector<std::pair<std::string, std::string>>>();
  withChain(
      [operation, lines](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        const auto &now = chain.value().meta;
        if (operation.kind == op::Kind::ConfigUpdate) {
          auto tx = pp::utl::binaryUnpack<pp::Ledger::TxConfig>(operation.record.data);
          pp::GenesisAccountMeta next;
          if (!tx || !next.ltsFromString(tx.value().meta)) {
            return "The update does not decode";
          }
          for (const auto &change : net::configChanges(now.config, next.config)) {
            lines->emplace_back(change.title, "from " + change.before + " to " + change.after);
          }
          const auto &a = now.genesis.wallet;
          const auto &b = next.genesis.wallet;
          if (a.publicKeys != b.publicKeys || a.minSignatures != b.minSignatures) {
            lines->emplace_back("Admin set", "from " + std::to_string(a.minSignatures) + " of " +
                                                 std::to_string(a.publicKeys.size()) + " to " +
                                                 std::to_string(b.minSignatures) + " of " +
                                                 std::to_string(b.publicKeys.size()) + " (keys below)");
          }
          if (lines->empty()) {
            lines->emplace_back("Changes", "none: the chain is already so");
          }
          if (now.hasPending) {
            lines->emplace_back("Refused while", "another update is pending (from epoch " +
                                                     op::groupDigits(now.pending.activationEpoch) + ")");
          }
        } else if (operation.kind == op::Kind::BeaconCertificate) {
          auto tx = pp::utl::binaryUnpack<pp::Ledger::TxBeaconCert>(operation.record.data);
          pp::BeaconCertificate cert;
          if (!tx || !cert.ltsFromString(tx.value().meta)) {
            return "The certificate does not decode";
          }
          lines->emplace_back("Generation", "from " + std::to_string(now.authority.beacon.generation) + " (in force) to " +
                                                std::to_string(cert.generation) +
                                                (cert.generation == now.authority.beacon.generation + 1
                                                     ? " (the next one)"
                                                     : " (not the next one: the chain refuses it)"));
        }
        return "";
      },
      [lines, done = std::move(done)](const std::string &error) {
        if (!error.empty()) {
          done({{"Cannot compare", error}});
          return;
        }
        done(*lines);
      });
}

void Online::load() {
  address_.clear();
  if (auto json = session_.vault.readFile(kSettingsFile); json && !json.value().empty()) {
    pp::common::Object root;
    if (pp::common::io::objectFromJsonString(root, json.value())) {
      address_ = root.getString("node").value_or("");
    }
  }
  if (onStatus) {
    onStatus(address_.empty() ? "No node set" : "");
  }
}

pp::Roe<void> Online::saveAddress(const std::string &address) {
  pp::common::Object root;
  root.set("node", address);
  if (auto saved = session_.vault.writeFile(kSettingsFile, pp::common::io::objectToJsonString(root)); !saved) {
    return saved;
  }
  address_ = address;
  return {};
}

std::string Online::linkLocked() {
  if (address_.empty()) {
    return "No node set: add one under Network";
  }
  if (link_.connected() && link_.address() == address_) {
    return "";
  }
  auto linked = link_.connect(address_);
  return linked ? "" : linked.error().message;
}

void Online::withChain(std::function<std::string(net::ChainLink &)> work,
                       std::function<void(const std::string &)> done) {
  auto error = std::make_shared<std::string>();
  app_.runInBackground(
      [this, work = std::move(work), error] {
        std::lock_guard lock(mutex_);
        *error = linkLocked();
        if (error->empty()) {
          *error = work(link_);
        }
      },
      [error, done = std::move(done)] { done(*error); });
}

void Online::submit(const op::Operation &operation,
                    std::function<void(const ui_host::Session::SubmitProgress &)> report) {
  auto progress = [this, report](std::string status, bool done, bool failed) {
    app_.post([report, status = std::move(status), done, failed] { report({status, done, failed}); });
  };
  app_.runInBackground(
      [this, operation, progress] {
        {
          std::lock_guard lock(mutex_);
          std::string error = linkLocked();
          if (error.empty()) {
            auto sent = link_.submit(operation);
            error = sent ? "" : sent.error().message;
          }
          if (!error.empty()) {
            progress(error, false, true);
            return;
          }
        }
        progress("Sent. Waiting for it to be in a block…", false, false);
        auto summary = op::summarize(operation);
        const int64_t until = summary && summary.value().validUntil != 0 ? summary.value().validUntil
                                                                         : nowSeconds() + 3600;
        while (app_.running() && nowSeconds() <= until + 60) {
          for (int i = 0; i < 50 && app_.running(); ++i) { // ~5 s, leaving promptly on exit
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
          }
          std::lock_guard lock(mutex_);
          if (!linkLocked().empty()) {
            continue; // the node may be back next time
          }
          if (auto in = link_.included(operation); in && in.value()) {
            progress("In a block: done.", true, false);
            return;
          }
        }
        if (app_.running()) {
          progress("Not in a block before its deadline: it did not go through. Make it again.", false, true);
        }
      },
      [] {});
}

} // namespace admin::online
