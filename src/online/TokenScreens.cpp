#include "online/TokenScreens.h"

#include "net/TokenOperations.h"
#include "core/AddressBook.h"
#include "core/Amounts.h"
#include "core/KeyCard.h"
#include "core/Operations.h"
#include "client/AccountIds.h"
#include "lib/common/Utilities.h"
#include "lib/common/BinaryPack.hpp"
#include "common/io/Json.h"

#include <algorithm>
#include <memory>

namespace apps::online {

namespace op = pp::operation;

namespace {

const std::vector<ui_host::FileFilter> kKeyCards = {{"Key cards", "json"}};
const char *kNoIssuer = "Set the issuer account under Overview first.";

net::Making making(int64_t validFor, const std::string &notes) { return {nowSeconds(), validFor, notes}; }

std::string nameOf(const std::string &publicKey, ui_host::Session &session) {
  const auto book = AddressBook::load(session.vault);
  const auto keys = session.vault.keys();
  return signerName(publicKey, book ? book.value() : AddressBook{}, keys ? keys.value() : std::vector<KeyInfo>{});
}

std::string signed64(int64_t value) {
  return value < 0 ? "-" + formatAmount(static_cast<uint64_t>(-(value + 1)) + 1) : formatAmount(static_cast<uint64_t>(value));
}

/** The issuer's account and its signers, read from the chain. */
struct Held {
  pp::UserAccount state;
  op::SignerSet signers;
};

pp::Roe<Held> readIssuer(net::ChainLink &link, uint64_t account) {
  auto state = link.account(account);
  if (!state) {
    return state.error();
  }
  Held held;
  held.state = std::move(state.value());
  held.signers.account = account;
  held.signers.publicKeys = held.state.wallet.publicKeys;
  held.signers.minSignatures = held.state.wallet.minSignatures;
  return held;
}

} // namespace

// ---- Issuer ----

void Issuer::load() {
  account_.reset();
  if (auto json = session_.vault.readFile(kSettingsFile); json && !json.value().empty()) {
    pp::common::Object root;
    if (pp::common::io::objectFromJsonString(root, json.value())) {
      if (auto id = root.getNonNegInt("account")) {
        account_ = *id;
      }
    }
  }
}

pp::Roe<void> Issuer::save(uint64_t account) {
  pp::common::Object root;
  root.setJsonUInt("account", account);
  if (auto saved = session_.vault.writeFile(kSettingsFile, pp::common::io::objectToJsonString(root)); !saved) {
    return saved;
  }
  account_ = account;
  return {};
}

// ---- Overview ----

void TokenOverviewScreen::bind(ui::DataModelConstructor &model) {
  if (auto fact = model.RegisterStruct<Fact>()) {
    fact.RegisterMember("label", &Fact::label);
    fact.RegisterMember("value", &Fact::value);
  }
  if (auto activity = model.RegisterStruct<Activity>()) {
    activity.RegisterMember("what", &Activity::what);
    activity.RegisterMember("fee", &Activity::fee);
  }
  model.RegisterArray<std::vector<Fact>>();
  model.RegisterArray<std::vector<Activity>>();
  model.RegisterArray<std::vector<std::string>>();
  model.Bind("account", &account_);
  model.Bind("known", &known_);
  model.Bind("facts", &facts_);
  model.Bind("signers", &signers_);
  model.Bind("recent", &recent_);
  model.Bind("warning", &warning_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
  model.BindEventCallback("token_save", &TokenOverviewScreen::saveAccount, this);
}

void TokenOverviewScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void TokenOverviewScreen::willShow() {
  facts_.clear();
  signers_.clear();
  recent_.clear();
  warning_.clear();
  const auto account = issuer_.account();
  known_ = account.has_value();
  if (!account) {
    account_.clear();
    say("", "Type the number of the issuer account you operate: the registrar gave it when it created the account.");
    return;
  }
  account_ = std::to_string(*account);
  say("", "Reading the account…");
  auto overview = std::make_shared<net::TokenOverview>();
  // The epoch's length and the epoch now, by the chain's clock: when an announced recovery applies.
  auto epochs = std::make_shared<std::pair<uint64_t, uint64_t>>(0, 0);
  const uint64_t id = *account;
  online_.withChain(
      [overview, epochs, id](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        *epochs = {net::epochSeconds(chain.value()), net::epochAt(chain.value(), nowSeconds())};
        auto state = link.account(id);
        if (!state) {
          return state.error().message;
        }
        auto transactions = link.transactions(id);
        auto read = net::tokenOverview(chain.value(), id, state.value(),
                                       transactions ? transactions.value() : std::vector<pp::Ledger::Record>{});
        if (!read) {
          return read.error().message;
        }
        *overview = std::move(read.value());
        return "";
      },
      [this, overview, epochs](const std::string &error) {
        if (!error.empty()) {
          say(error + ".");
          return;
        }
        const auto &o = *overview;
        const uint64_t native = o.native < 0 ? 0 : static_cast<uint64_t>(o.native);
        facts_ = {{"Its token", (o.domain.empty() ? "" : o.domain + ", ") + "token " + formatAmount(o.account) +
                                    " (the account's number)"},
                  {"Its symbol", o.domain.empty() ? "none yet: the registrar gives it (its domain)" : o.domain},
                  {"Issued and out", formatAmount(o.supply)},
                  {"Native balance", formatAmount(native)},
                  {"A renewal costs about", formatAmount(o.renewalFee)},
                  {"Signed by", std::to_string(o.minSignatures) + " of " + std::to_string(o.publicKeys.size())}};
        facts_.push_back({"Recovery", o.recovery.hasSet() ? recoveryText(o.recovery) : "none"});
        for (const auto &[token, balance] : o.otherTokens) {
          facts_.push_back({"Holds token " + formatAmount(token), signed64(balance)});
        }
        if (o.low) {
          const uint64_t left = o.renewalFee == 0 ? 0 : native / o.renewalFee;
          warning_ = "The native balance covers about " + std::to_string(left) +
                     " renewals. An account that cannot pay its renewal is ended, and its token's book with it: "
                     "ask the registrar (or anyone) to send it native tokens.";
        }
        if (const std::string announced =
                announcedText(o.recovery, epochs->second, epochs->first,
                              "If the issuer did not ask for it, its recovery keys' holders withdraw it (Signers).");
            !announced.empty()) {
          warning_ += (warning_.empty() ? "" : " ") + announced;
        }
        for (const auto &key : o.publicKeys) {
          signers_.push_back(nameOf(key, session_));
        }
        for (const auto &activity : o.recent) {
          recent_.push_back({activity.what, activity.fee});
        }
        say("");
      });
}

void TokenOverviewScreen::saveAccount(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (busy_) {
    return;
  }
  auto id = parseAmount(account_);
  if (!id || !net::isIssuedId(id.value())) {
    say("Type the issuer account's number (from " + formatAmount(pp::AccountIds::ID_FIRST_ISSUED) + " to " +
        formatAmount(pp::AccountIds::ID_FIRST_USER - 1) + ").");
    return;
  }
  busy_ = true;
  say("", "Looking for the account…");
  const uint64_t account = id.value();
  online_.withChain(
      [account](net::ChainLink &link) -> std::string {
        auto state = link.account(account);
        return state ? "" : state.error().message;
      },
      [this, account](const std::string &error) {
        busy_ = false;
        if (!error.empty()) {
          say(error + ".");
          return;
        }
        if (auto saved = issuer_.save(account); !saved) {
          say(saved.error().message);
          return;
        }
        willShow();
      });
}

// ---- Issue ----

void IssueScreen::bind(ui::DataModelConstructor &model) {
  bindForm(model);
  model.Bind("issuer", &issuerName_);
  model.Bind("supply", &supply_);
  model.Bind("to", &to_);
  model.Bind("amount", &amount_);
  model.Bind("fee", &fee_);
  model.BindEventCallback("issue_make", &IssueScreen::makeIssue, this);
}

void IssueScreen::clearForm() {
  to_.clear();
  amount_.clear();
}

void IssueScreen::willShow() {
  const auto account = issuer_.account();
  if (!account) {
    issuerName_ = "not set";
    supply_ = "unknown";
    say(kNoIssuer);
    return;
  }
  issuerName_ = formatAmount(*account);
  supply_ = "…";
  say("");
  auto supply = std::make_shared<std::string>();
  auto fee = std::make_shared<std::string>();
  const uint64_t id = *account;
  online_.withChain(
      [id, supply, fee](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        auto held = readIssuer(link, id);
        if (!held) {
          return held.error().message;
        }
        auto overview = net::tokenOverview(chain.value(), id, held.value().state, {});
        if (!overview) {
          return overview.error().message;
        }
        *supply = formatAmount(overview.value().supply);
        // An issue's least fee (a plain transfer's: no metadata): what the form starts from.
        pp::Ledger::TxDefault probe;
        probe.tokenId = id;
        probe.fromWalletId = id;
        pp::Ledger::Record record;
        record.type = pp::Ledger::T_DEFAULT;
        record.data = pp::utl::binaryPack(probe);
        if (auto least = link.minimumFee(record)) {
          *fee = formatAmount(least.value());
        }
        return "";
      },
      [this, supply, fee](const std::string &error) {
        supply_ = error.empty() ? *supply : "unknown";
        if (fee_.empty()) {
          fee_ = *fee;
        }
        say(error.empty() ? "" : error + ".");
      });
}

void IssueScreen::makeIssue(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto account = issuer_.account();
  if (!account) {
    say(kNoIssuer);
    return;
  }
  auto to = parseAmount(to_);
  auto amount = parseAmount(amount_);
  auto fee = parseAmount(fee_);
  if (!to) {
    say("Type the account the tokens go to (its number).");
    return;
  }
  if (!amount || amount.value() == 0) {
    say(amount ? "Type an amount above 0." : amount.error().message + ".");
    return;
  }
  if (!fee) {
    say("Type the fee: " + fee.error().message + ".");
    return;
  }
  const uint64_t id = *account;
  const uint64_t toId = to.value();
  const uint64_t amountValue = amount.value();
  const uint64_t feeValue = fee.value();
  make(
      [=](net::ChainLink &link, const net::ChainInfo &chain, int64_t validFor,
          const std::string &notes) -> pp::Roe<op::Operation> {
        auto held = readIssuer(link, id);
        if (!held) {
          return held.error();
        }
        if (!link.account(toId)) {
          return pp::Error("Account " + formatAmount(toId) + " is not on the chain");
        }
        return net::makeIssue(chain, held.value().signers, held.value().state, toId, amountValue, feeValue,
                              making(validFor, notes));
      },
      "issue-" + std::to_string(amountValue) + "-to-" + std::to_string(toId) + ".json");
}

// ---- Send ----

void SendScreen::bind(ui::DataModelConstructor &model) {
  bindForm(model);
  model.Bind("holdings", &holdings_);
  model.Bind("token", &token_);
  model.Bind("to", &to_);
  model.Bind("amount", &amount_);
  model.Bind("fee", &fee_);
  model.BindEventCallback("send_make", &SendScreen::makeSend, this);
}

void SendScreen::clearForm() {
  to_.clear();
  amount_.clear();
}

void SendScreen::willShow() {
  const auto account = issuer_.account();
  if (!account) {
    holdings_ = "unknown";
    say(kNoIssuer);
    return;
  }
  holdings_ = "…";
  say("");
  auto holdings = std::make_shared<std::string>();
  auto fee = std::make_shared<std::string>();
  const uint64_t id = *account;
  online_.withChain(
      [id, holdings, fee](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        auto held = readIssuer(link, id);
        if (!held) {
          return held.error().message;
        }
        std::string text;
        for (const auto &[token, balance] : held.value().state.wallet.mBalances) {
          if (token == id) {
            continue; // its own: issued (Issue)
          }
          text += (text.empty() ? "" : " · ") +
                  (token == pp::AccountIds::ID_GENESIS ? std::string("native") : "token " + formatAmount(token)) +
                  " " + signed64(balance);
        }
        *holdings = text.empty() ? "nothing" : text;
        pp::Ledger::TxDefault probe;
        probe.fromWalletId = id;
        pp::Ledger::Record record;
        record.type = pp::Ledger::T_DEFAULT;
        record.data = pp::utl::binaryPack(probe);
        if (auto least = link.minimumFee(record)) {
          *fee = formatAmount(least.value());
        }
        return "";
      },
      [this, holdings, fee](const std::string &error) {
        holdings_ = error.empty() ? *holdings : "unknown";
        if (fee_.empty()) {
          fee_ = *fee;
        }
        say(error.empty() ? "" : error + ".");
      });
}

void SendScreen::makeSend(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto account = issuer_.account();
  if (!account) {
    say(kNoIssuer);
    return;
  }
  uint64_t token = pp::AccountIds::ID_GENESIS;
  if (!token_.empty()) {
    auto typed = parseAmount(token_);
    if (!typed) {
      say("Type the token's number, or leave it empty for the native token.");
      return;
    }
    token = typed.value();
  }
  auto to = parseAmount(to_);
  auto amount = parseAmount(amount_);
  auto fee = parseAmount(fee_);
  if (!to) {
    say("Type the account it goes to (its number).");
    return;
  }
  if (!amount || amount.value() == 0) {
    say(amount ? "Type an amount above 0." : amount.error().message + ".");
    return;
  }
  if (!fee) {
    say("Type the fee: " + fee.error().message + ".");
    return;
  }
  const uint64_t id = *account;
  const uint64_t toId = to.value();
  const uint64_t amountValue = amount.value();
  const uint64_t feeValue = fee.value();
  make(
      [=](net::ChainLink &link, const net::ChainInfo &chain, int64_t validFor,
          const std::string &notes) -> pp::Roe<op::Operation> {
        auto held = readIssuer(link, id);
        if (!held) {
          return held.error();
        }
        return net::makeSend(chain, held.value().signers, held.value().state, toId, token, amountValue, feeValue,
                             making(validFor, notes));
      },
      "send-" + std::to_string(amountValue) + (token == 0 ? "" : "-of-" + std::to_string(token)) + "-to-" +
          std::to_string(toId) + ".json");
}

// ---- Signers ----

void SignersScreen::bind(ui::DataModelConstructor &model) {
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
  model.Bind("fee", &fee_);
  model.BindEventCallback("signers_card", &SignersScreen::addCard, this);
  model.BindEventCallback("signers_fee", &SignersScreen::updateFee, this);
  model.BindEventCallback("signers_make", &SignersScreen::makeReplacement, this);
  if (auto row = model.RegisterStruct<ChosenKeys::Row>()) {
    row.RegisterMember("name", &ChosenKeys::Row::name);
    row.RegisterMember("fingerprint", &ChosenKeys::Row::fingerprint);
  }
  model.RegisterArray<std::vector<ChosenKeys::Row>>();
  model.Bind("recovery_now", &recoveryNow_);
  model.Bind("announced", &announced_);
  model.Bind("has_recovery", &hasRecovery_);
  model.Bind("is_announced", &isAnnounced_);
  model.Bind("recovery_keys", &recoveryKeys_.rows);
  model.Bind("recovery_needed", &recoveryNeeded_);
  model.Bind("recovery_days", &recoveryDays_);
  model.BindEventCallback("recovery_card", &SignersScreen::addRecoveryCard, this);
  model.BindEventCallback("recovery_drop", &SignersScreen::dropRecoveryKey, this);
  model.BindEventCallback("recovery_set", &SignersScreen::makeRecoverySet, this);
  model.BindEventCallback("recovery_remove", &SignersScreen::makeRecoveryRemoval, this);
  model.BindEventCallback("recovery_withdraw", &SignersScreen::makeRecoveryWithdrawal, this);
}

void SignersScreen::clearForm() {
  fee_.clear();
  recoveryKeys_.clear();
}

void SignersScreen::addCandidate(const std::string &publicKey, bool chosen) {
  if (std::find(candidateKeys_.begin(), candidateKeys_.end(), publicKey) != candidateKeys_.end()) {
    return;
  }
  candidates_.push_back({nameOf(publicKey, session_), op::fingerprint(publicKey), chosen});
  candidateKeys_.push_back(publicKey);
}

std::vector<std::string> SignersScreen::chosenKeys() const {
  std::vector<std::string> keys;
  for (size_t i = 0; i < candidates_.size(); ++i) {
    if (candidates_[i].chosen) {
      keys.push_back(candidateKeys_[i]);
    }
  }
  return keys;
}

void SignersScreen::willShow() {
  candidates_.clear();
  candidateKeys_.clear();
  const auto account = issuer_.account();
  if (!account) {
    current_ = "unknown";
    say(kNoIssuer);
    return;
  }
  say("", "Reading the keys in force…");
  auto wallet = std::make_shared<pp::Wallet>();
  // The epoch's length and the epoch now, by the chain's clock: when an announced recovery applies.
  auto epochs = std::make_shared<std::pair<uint64_t, uint64_t>>(0, 0);
  const uint64_t id = *account;
  online_.withChain(
      [id, wallet, epochs](net::ChainLink &link) -> std::string {
        auto held = readIssuer(link, id);
        if (!held) {
          return held.error().message;
        }
        *wallet = held.value().state.wallet;
        if (auto chain = link.info()) {
          *epochs = {net::epochSeconds(chain.value()), net::epochAt(chain.value(), nowSeconds())};
        }
        return "";
      },
      [this, wallet, epochs](const std::string &error) {
        const auto &r = wallet->recovery;
        hasRecovery_ = r.hasSet();
        isAnnounced_ = r.pending;
        recoveryNow_ = error.empty() ? (hasRecovery_ ? "Recovery: " + recoveryText(r) +
                                                           ". Its recovery keys: " + fingerprintsText(r.publicKeys) +
                                                           ". Only they change it now."
                                                     : "No recovery: if every key in force is lost, so is the account.")
                                     : "";
        announced_ = announcedText(r, epochs->second, epochs->first,
                                   "If the issuer did not ask for it, its recovery keys' holders withdraw it.");
        // The keys in force, chosen; then the owners' keys in the vault and the address book.
        for (const auto &publicKey : wallet->publicKeys) {
          addCandidate(publicKey, true);
        }
        if (auto keys = session_.vault.keys()) {
          for (const auto &key : keys.value()) {
            if (key.role == Role::Owner) {
              addCandidate(key.publicKey, false);
            }
          }
        }
        if (auto book = AddressBook::load(session_.vault)) {
          for (const auto &card : book.value().cards()) {
            if (card.role == Role::Owner) {
              addCandidate(card.publicKey, false);
            }
          }
        }
        current_ = error.empty() ? std::to_string(wallet->minSignatures) + " of " +
                                       std::to_string(wallet->publicKeys.size()) + " sign"
                                 : "unknown";
        threshold_ = std::to_string(wallet->minSignatures == 0 ? 1 : wallet->minSignatures);
        say(error.empty() ? "" : error + ".");
      });
}

void SignersScreen::addCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  app_->chooseFileToOpen(kKeyCards, [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    auto card = loadKeyCard(*path);
    if (!card) {
      say(card.error().message);
      return;
    }
    addCandidate(card.value().publicKey, true);
    say("", "Check " + card.value().name + "'s fingerprint with them.");
  });
}

void SignersScreen::updateFee(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto account = issuer_.account();
  const auto keys = chosenKeys();
  auto threshold = parseAmount(threshold_);
  if (!account || keys.empty() || !threshold || threshold.value() == 0 || threshold.value() > keys.size()) {
    say("Choose the keys and how many must sign first.");
    return;
  }
  auto fee = std::make_shared<uint64_t>(0);
  const uint64_t id = *account;
  const auto min = static_cast<uint32_t>(threshold.value());
  online_.withChain(
      [id, keys, min, fee](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        auto held = readIssuer(link, id);
        if (!held) {
          return held.error().message;
        }
        auto least = net::keyReplacementFee(chain.value(), held.value().signers, held.value().state, keys, min);
        if (!least) {
          return least.error().message;
        }
        *fee = least.value();
        return "";
      },
      [this, fee](const std::string &error) {
        if (error.empty()) {
          fee_ = formatAmount(*fee);
        }
        say(error.empty() ? "" : error + ".");
      });
}

void SignersScreen::makeReplacement(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto account = issuer_.account();
  if (!account) {
    say(kNoIssuer);
    return;
  }
  const auto keys = chosenKeys();
  auto threshold = parseAmount(threshold_);
  if (!threshold || threshold.value() == 0 || threshold.value() > keys.size() || threshold.value() > 255) {
    say("Choose how many must sign: between 1 and the keys chosen.");
    return;
  }
  auto fee = parseAmount(fee_);
  if (!fee) {
    say("Type the fee, or work it out: " + fee.error().message + ".");
    return;
  }
  const uint64_t id = *account;
  const auto min = static_cast<uint32_t>(threshold.value());
  const uint64_t feeValue = fee.value();
  make(
      [=](net::ChainLink &link, const net::ChainInfo &chain, int64_t validFor,
          const std::string &notes) -> pp::Roe<op::Operation> {
        auto held = readIssuer(link, id);
        if (!held) {
          return held.error();
        }
        return net::makeKeyReplacement(chain, held.value().signers, held.value().state, keys, min, feeValue,
                                       making(validFor, notes));
      },
      "keys-" + std::to_string(id) + ".json");
}

// ---- Its recovery ----

void SignersScreen::addRecoveryCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  app_->chooseFileToOpen(kKeyCards, [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    auto card = loadKeyCard(*path);
    if (!card) {
      say(card.error().message);
      return;
    }
    const std::string why = recoveryKeys_.add(card.value().name, card.value().publicKey);
    say(why, why.empty() ? "Check " + card.value().name + "'s fingerprint with them." : "");
  });
}

void SignersScreen::dropRecoveryKey(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
  recoveryKeys_.drop(args.empty() ? -1 : args[0].Get<int>(-1));
  refresh();
}

void SignersScreen::makeRecoverySet(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto account = issuer_.account();
  if (!account) {
    say(kNoIssuer);
    return;
  }
  const auto keys = recoveryKeys_.keys;
  auto needed = parseAmount(recoveryNeeded_);
  auto days = parseAmount(recoveryDays_);
  if (keys.empty()) {
    say("Add the recovery keys' cards: people the issuer trusts, or backup keys kept apart from these.");
    return;
  }
  if (!needed || needed.value() == 0 || needed.value() > keys.size()) {
    say("How many recovery keys must sign: between 1 and the keys chosen.");
    return;
  }
  if (!days) {
    say("Type the delay in days.");
    return;
  }
  const uint64_t id = *account;
  const auto min = static_cast<uint32_t>(needed.value());
  const uint64_t d = days.value();
  // The keys in force name the first set; a change after that is the set's to sign.
  make(
      [=](net::ChainLink &link, const net::ChainInfo &chain, int64_t validFor,
          const std::string &notes) -> pp::Roe<op::Operation> {
        auto held = readIssuer(link, id);
        if (!held) {
          return held.error();
        }
        auto delay = net::delaySecondsFor(chain, d);
        if (!delay) {
          return delay.error();
        }
        const auto &state = held.value().state;
        if (state.wallet.recovery.hasSet()) {
          return net::makeRecoveryChange(chain, id, state, keys, min, delay.value(), making(validFor, notes));
        }
        return net::makeRecoverySet(chain, held.value().signers, state, keys, min, delay.value(),
                                    making(validFor, notes));
      },
      "recovery-" + std::to_string(id) + ".json");
}

void SignersScreen::makeRecoveryRemoval(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto account = issuer_.account();
  if (!account) {
    say(kNoIssuer);
    return;
  }
  const uint64_t id = *account;
  make(
      [=](net::ChainLink &link, const net::ChainInfo &chain, int64_t validFor,
          const std::string &notes) -> pp::Roe<op::Operation> {
        auto held = readIssuer(link, id);
        if (!held) {
          return held.error();
        }
        return net::makeRecoveryChange(chain, id, held.value().state, {}, 0, 0, making(validFor, notes));
      },
      "no-recovery-" + std::to_string(id) + ".json");
}

void SignersScreen::makeRecoveryWithdrawal(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto account = issuer_.account();
  if (!account) {
    say(kNoIssuer);
    return;
  }
  const uint64_t id = *account;
  make(
      [=](net::ChainLink &link, const net::ChainInfo &chain, int64_t validFor,
          const std::string &notes) -> pp::Roe<op::Operation> {
        auto held = readIssuer(link, id);
        if (!held) {
          return held.error();
        }
        return net::makeRecoveryWithdrawal(chain, id, held.value().state, making(validFor, notes));
      },
      "withdraw-recovery-" + std::to_string(id) + ".json");
}

} // namespace apps::online
