#include "online/AdminScreens.h"

#include "net/AdminOperations.h"
#include "core/AddressBook.h"
#include "core/Amounts.h"
#include "core/Operations.h"
#include "lib/common/BinaryPack.hpp"
#include "lib/common/Utilities.h"
#include "client/AccountIds.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <cctype>

namespace apps::online {

namespace op = pp::operation;

namespace {

const std::vector<ui_host::FileFilter> kKeyCards = {{"Key cards", "json"}};

net::Making making(int64_t validFor, const std::string &notes) { return {nowSeconds(), validFor, notes}; }

/** Names for keys, from the address book and the vault. */
std::string nameOf(const std::string &publicKey, ui_host::Session &session) {
  const auto book = AddressBook::load(session.vault);
  const auto keys = session.vault.keys();
  return signerName(publicKey, book ? book.value() : AddressBook{}, keys ? keys.value() : std::vector<KeyInfo>{});
}

} // namespace

// ---- Mint ----

void MintScreen::bind(ui::DataModelConstructor &model) {
  bindForm(model);
  model.Bind("cap", &cap_);
  model.Bind("reserve", &reserve_);
  model.Bind("amount", &amount_);
  model.Bind("fee", &fee_);
  model.BindEventCallback("mint_make", &MintScreen::makeMint, this);
}

void MintScreen::willShow() {
  cap_ = "…";
  reserve_ = "…";
  say("");
  auto cap = std::make_shared<std::string>();
  auto reserve = std::make_shared<std::string>();
  auto fee = std::make_shared<std::string>();
  online_.withChain(
      [cap, reserve, fee](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        const uint64_t most = chain.value().meta.config.maxIssuancePerEpoch;
        *cap = most == 0 ? "nothing (the cap is 0: raise it under Config first)" : formatAmount(most);
        if (auto account = link.account(pp::AccountIds::ID_RESERVE)) {
          const auto &balances = account.value().wallet.mBalances;
          const auto native = balances.find(pp::AccountIds::ID_GENESIS);
          *reserve = formatAmount(native == balances.end() || native->second < 0 ? 0
                                                                                 : static_cast<uint64_t>(native->second));
        }
        pp::Ledger::TxDefault probe;
        probe.fromWalletId = pp::AccountIds::ID_GENESIS;
        probe.toWalletId = pp::AccountIds::ID_RESERVE;
        pp::Ledger::Record record;
        record.type = pp::Ledger::T_DEFAULT;
        record.data = pp::utl::binaryPack(probe);
        if (auto least = link.minimumFee(record)) {
          *fee = formatAmount(least.value());
        }
        return "";
      },
      [this, cap, reserve, fee](const std::string &error) {
        cap_ = error.empty() ? *cap : "unknown";
        reserve_ = error.empty() ? *reserve : "unknown";
        if (fee_.empty()) {
          fee_ = *fee;
        }
        say(error.empty() ? "" : error + ".");
      });
}

void MintScreen::makeMint(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  auto amount = parseAmount(amount_);
  auto fee = parseAmount(fee_);
  if (!amount || amount.value() == 0) {
    say(amount ? "Type an amount above 0." : amount.error().message + ".");
    return;
  }
  if (!fee) {
    say("Type the fee: " + fee.error().message + ".");
    return;
  }
  const uint64_t amountValue = amount.value();
  const uint64_t feeValue = fee.value();
  make(
      [=](net::ChainLink &, const net::ChainInfo &chain, int64_t validFor, const std::string &notes) {
        return net::makeMint(chain, amountValue, feeValue, making(validFor, notes));
      },
      "mint-" + std::to_string(amountValue) + ".json");
}

// ---- Config ----

void ConfigScreen::bind(ui::DataModelConstructor &model) {
  bindForm(model);
  if (auto row = model.RegisterStruct<Row>()) {
    row.RegisterMember("name", &Row::name);
    row.RegisterMember("title", &Row::title);
    row.RegisterMember("current", &Row::current);
    row.RegisterMember("next", &Row::next);
  }
  model.RegisterArray<std::vector<Row>>();
  model.Bind("rows", &rows_);
  model.Bind("pending", &pending_);
  model.BindEventCallback("config_make", &ConfigScreen::makeUpdate, this);
}

void ConfigScreen::willShow() {
  say("", "Reading the config in force…");
  auto rows = std::make_shared<std::vector<Row>>();
  auto pending = std::make_shared<std::string>();
  online_.withChain(
      [rows, pending](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        const auto &config = chain.value().meta.config;
        for (const auto &field : net::configFields()) {
          const std::string value = formatAmount(field.get(config));
          rows->push_back({field.name, field.title, value, value});
        }
        if (chain.value().meta.hasPending) {
          *pending = "An update is pending: it applies from epoch " +
                     formatAmount(chain.value().meta.pending.activationEpoch) +
                     ". Another can be made once it does.";
        }
        return "";
      },
      [this, rows, pending](const std::string &error) {
        rows_ = *rows;
        pending_ = *pending;
        say(error.empty() ? "" : error + ".");
      });
}

void ConfigScreen::makeUpdate(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  std::vector<std::pair<std::string, uint64_t>> values;
  for (const auto &row : rows_) {
    auto value = parseAmount(row.next);
    if (!value) {
      say(row.title + ": " + value.error().message + ".");
      return;
    }
    values.emplace_back(row.name, value.value());
  }
  make(
      [values](net::ChainLink &, const net::ChainInfo &chain, int64_t validFor, const std::string &notes) {
        pp::BlockChainConfig next = chain.meta.config;
        for (const auto &[name, value] : values) {
          for (const auto &field : net::configFields()) {
            if (field.name == name) {
              field.set(next, value);
            }
          }
        }
        return net::makeConfigUpdate(chain, next, std::nullopt, making(validFor, notes));
      },
      "config-update.json");
}

// ---- Admin keys ----

void AdminKeysScreen::bind(ui::DataModelConstructor &model) {
  bindForm(model);
  if (auto candidate = model.RegisterStruct<Candidate>()) {
    candidate.RegisterMember("name", &Candidate::name);
    candidate.RegisterMember("fingerprint", &Candidate::fingerprint);
    candidate.RegisterMember("chosen", &Candidate::chosen);
  }
  model.RegisterArray<std::vector<Candidate>>();
  model.Bind("current", &current_);
  model.Bind("candidates", &candidates_);
  model.Bind("threshold", &threshold_);
  model.BindEventCallback("admins_make", &AdminKeysScreen::makeRotation, this);
}

void AdminKeysScreen::willShow() {
  say("", "Reading the admin set in force…");
  auto admins = std::make_shared<pp::Wallet>();
  online_.withChain(
      [admins](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        *admins = chain.value().meta.genesis.wallet;
        return "";
      },
      [this, admins](const std::string &error) {
        candidates_.clear();
        candidateKeys_.clear();
        auto add = [this](const std::string &publicKey, bool chosen) {
          if (std::find(candidateKeys_.begin(), candidateKeys_.end(), publicKey) != candidateKeys_.end()) {
            return;
          }
          candidates_.push_back({nameOf(publicKey, session_), op::fingerprint(publicKey), chosen});
          candidateKeys_.push_back(publicKey);
        };
        // The set in force, chosen; then the admin keys people have handed their cards for.
        for (const auto &publicKey : admins->publicKeys) {
          add(publicKey, true);
        }
        if (auto book = AddressBook::load(session_.vault)) {
          for (const auto &card : book.value().cards()) {
            if (card.role == Role::Admin) {
              add(card.publicKey, false);
            }
          }
        }
        if (auto keys = session_.vault.keys()) {
          for (const auto &key : keys.value()) {
            if (key.role == Role::Admin) {
              add(key.publicKey, false);
            }
          }
        }
        current_ = error.empty() ? std::to_string(admins->minSignatures) + " of " +
                                       std::to_string(admins->publicKeys.size()) + " sign"
                                 : "unknown";
        threshold_ = std::to_string(admins->minSignatures == 0 ? 2 : admins->minSignatures);
        say(error.empty() ? "" : error + ".");
      });
}

void AdminKeysScreen::makeRotation(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  pp::Wallet next;
  for (size_t i = 0; i < candidates_.size(); ++i) {
    if (candidates_[i].chosen) {
      next.publicKeys.push_back(candidateKeys_[i]);
    }
  }
  auto threshold = parseAmount(threshold_);
  if (!threshold || threshold.value() == 0 || threshold.value() > next.publicKeys.size() || threshold.value() > 255) {
    say("Choose how many must sign: between 1 and the keys chosen.");
    return;
  }
  next.minSignatures = static_cast<uint8_t>(threshold.value());
  make(
      [next](net::ChainLink &, const net::ChainInfo &chain, int64_t validFor, const std::string &notes) {
        return net::makeConfigUpdate(chain, chain.meta.config, next, making(validFor, notes));
      },
      "admin-keys.json");
}

// ---- Beacon ----

void BeaconScreen::bind(ui::DataModelConstructor &model) {
  bindForm(model);
  model.Bind("current", &current_);
  model.Bind("next", &next_);
  model.Bind("chosen", &chosen_);
  model.Bind("hints", &hints_);
  model.BindEventCallback("beacon_card", &BeaconScreen::chooseCard, this);
  model.BindEventCallback("beacon_make", &BeaconScreen::makeCertificate, this);
}

void BeaconScreen::clearForm() {
  chosen_.clear();
  chosenKey_.clear();
  hints_.clear();
}

void BeaconScreen::willShow() {
  say("");
  auto authority = std::make_shared<pp::ChainAuthority>();
  online_.withChain(
      [authority](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        *authority = chain.value().meta.authority;
        return "";
      },
      [this, authority](const std::string &error) {
        if (!error.empty()) {
          current_ = "unknown";
          say(error + ".");
          return;
        }
        current_ = "generation " + std::to_string(authority->beacon.generation) + ", key " +
                   nameOf(authority->beacon.publicKey, session_);
        next_ = std::to_string(authority->beacon.generation + 1);
        refresh();
      });
}

void BeaconScreen::chooseCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  app_->chooseFileToOpen(kKeyCards, [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    auto card = loadKeyCard(*path);
    if (!card) {
      say(card.error().message);
      return;
    }
    if (card.value().role != Role::Beacon) {
      say("That is a " + roleTitle(card.value().role) + " key card, not a beacon's.");
      return;
    }
    chosenKey_ = card.value().publicKey;
    chosen_ = card.value().name + " (" + op::fingerprint(chosenKey_) + ")";
    say("", "Check the fingerprint with the beacon's operator.");
  });
}

void BeaconScreen::makeCertificate(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (chosenKey_.empty()) {
    say("Choose the new beacon key's card first.");
    return;
  }
  std::vector<std::string> hints;
  std::istringstream lines(hints_);
  for (std::string hint; std::getline(lines, hint, ',');) {
    hint.erase(std::remove_if(hint.begin(), hint.end(), [](unsigned char c) { return std::isspace(c); }), hint.end());
    if (!hint.empty()) {
      hints.push_back(hint);
    }
  }
  const std::string key = chosenKey_;
  make(
      [key, hints](net::ChainLink &, const net::ChainInfo &chain, int64_t validFor, const std::string &notes) {
        return net::makeBeaconCertificate(chain, key, hints, making(validFor, notes));
      },
      "beacon-certificate.json");
}

} // namespace apps::online
