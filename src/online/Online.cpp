#include "online/Online.h"

#include "common/io/Json.h"

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
  session_.submit = [this](const op::Operation &operation,
                           std::function<void(const ui_host::Session::SubmitProgress &)> report) {
    submit(operation, std::move(report));
  };
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
