#include "online/RegistrarScreen.h"

#include "net/AdminOperations.h"
#include "ledger/chain/Domains.h"
#include "core/AtomicFile.h"
#include "core/Amounts.h"
#include "core/KeyCard.h"
#include "ledger/client/AccountIds.h"
#include "ledger/lib/common/Utilities.h"

#include <sodium.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>

namespace apps::online {

namespace op = pp::operation;

namespace {

const std::vector<ui_host::FileFilter> kKeyCards = {{"Key cards", "json"}};
const std::vector<ui_host::FileFilter> kPubFiles = {{"Public keys (pp-client keygen)", "pub"}};

int indexArg(const ui::VariantList &args) { return args.empty() ? -1 : args[0].Get<int>(-1); }

/** `typed` as a domain (stored as capitals), or none when empty; why not, if it cannot be one. */
pp::Roe<std::string> domainOf(const std::string &typed) {
  std::string text;
  for (char c : typed) {
    if (c != ' ') {
      text.push_back(c);
    }
  }
  if (text.empty()) {
    return std::string();
  }
  auto name = pp::chain_domain::normalize(text);
  if (!name || name->size() < 4) {
    return pp::Error("An issued account's domain is 4 to 7 letters A-Z");
  }
  return *name;
}

/** `domain` is nobody's, as the node says (the chain refuses one that is). */
pp::Roe<void> checkFree(net::ChainLink &link, const std::string &domain) {
  auto proof = link.domainProof(domain);
  if (!proof) {
    return proof.error();
  }
  if (proof.value().present) {
    return pp::Error("Domain " + domain + " is account " + formatAmount(proof.value().accountId) + "'s");
  }
  return {};
}

} // namespace

void RegistrarScreen::bind(ui::DataModelConstructor &model) {
  bindForm(model);
  if (auto key = model.RegisterStruct<OwnerKey>()) {
    key.RegisterMember("name", &OwnerKey::name);
    key.RegisterMember("fingerprint", &OwnerKey::fingerprint);
  }
  model.RegisterArray<std::vector<OwnerKey>>();
  model.Bind("balance", &balance_);
  model.Bind("signers", &signers_);
  model.Bind("id", &id_);
  model.Bind("keys", &keys_);
  model.Bind("min_signatures", &minSignatures_);
  model.Bind("amount", &amount_);
  model.Bind("fee", &fee_);
  model.Bind("domain", &domain_);
  model.Bind("assign_id", &assignId_);
  model.Bind("assign_domain", &assignDomain_);
  model.BindEventCallback("registrar_pick_id", &RegistrarScreen::pickId, this);
  model.BindEventCallback("registrar_add_card", &RegistrarScreen::addCard, this);
  model.BindEventCallback("registrar_add_pub", &RegistrarScreen::addPub, this);
  model.BindEventCallback("registrar_remove_key", &RegistrarScreen::removeKey, this);
  model.BindEventCallback("registrar_make", &RegistrarScreen::makeAccount, this);
  model.BindEventCallback("registrar_assign", &RegistrarScreen::makeAssignment, this);
}

void RegistrarScreen::clearForm() {
  id_.clear();
  keys_.clear();
  keyBytes_.clear();
  minSignatures_ = "1";
  amount_.clear();
  fee_.clear();
  domain_.clear();
  assignId_.clear();
  assignDomain_.clear();
}

void RegistrarScreen::willShow() {
  balance_ = "…";
  signers_ = "…";
  say("");
  auto balance = std::make_shared<std::string>();
  auto signers = std::make_shared<std::string>();
  online_.withChain(
      [balance, signers](net::ChainLink &link) -> std::string {
        auto account = link.account(pp::AccountIds::ID_REGISTRAR);
        if (!account) {
          return account.error().message;
        }
        const auto &wallet = account.value().wallet;
        const auto native = wallet.mBalances.find(pp::AccountIds::ID_GENESIS);
        *balance = formatAmount(native == wallet.mBalances.end() || native->second < 0
                                    ? 0
                                    : static_cast<uint64_t>(native->second));
        *signers = std::to_string(wallet.minSignatures) + " of " + std::to_string(wallet.publicKeys.size());
        return "";
      },
      [this, balance, signers](const std::string &error) {
        balance_ = error.empty() ? *balance : "unknown";
        signers_ = error.empty() ? *signers : "unknown";
        say(error.empty() ? "" : error + ".");
      });
}

void RegistrarScreen::pickId(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  auto picked = std::make_shared<uint64_t>(0);
  say("", "Looking for a free number…");
  online_.withChain(
      [picked](net::ChainLink &link) -> std::string {
        constexpr uint64_t kSpan = pp::AccountIds::ID_FIRST_USER - pp::AccountIds::ID_FIRST_ISSUED;
        for (int attempt = 0; attempt < 8; ++attempt) {
          uint64_t r = 0;
          randombytes_buf(&r, sizeof(r));
          const uint64_t id = pp::AccountIds::ID_FIRST_ISSUED + r % kSpan;
          if (!link.account(id)) { // not on chain: free
            *picked = id;
            return "";
          }
        }
        return "No free number found: try again";
      },
      [this, picked](const std::string &error) {
        if (error.empty()) {
          id_ = std::to_string(*picked);
        }
        say(error.empty() ? "" : error + ".");
      });
}

void RegistrarScreen::addKey(const std::string &publicKey, const std::string &name) {
  if (std::find(keyBytes_.begin(), keyBytes_.end(), publicKey) != keyBytes_.end()) {
    say("That key is listed already.");
    return;
  }
  keys_.push_back({name, op::fingerprint(publicKey)});
  keyBytes_.push_back(publicKey);
  say("", "Check the fingerprint with the account's owner.");
  updateFee();
}

void RegistrarScreen::addCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  app_->chooseFileToOpen(kKeyCards, [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    auto card = loadKeyCard(*path);
    if (!card) {
      say(card.error().message);
      return;
    }
    addKey(card.value().publicKey, card.value().name);
  });
}

void RegistrarScreen::addPub(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  app_->chooseFileToOpen(kPubFiles, [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    auto text = readFile(*path);
    std::string hex = text ? text.value() : "";
    hex.erase(std::remove_if(hex.begin(), hex.end(), [](unsigned char c) { return std::isspace(c); }), hex.end());
    const std::string key = pp::utl::hexDecode(hex);
    if (key.size() != pp::utl::kMlDsaPublicKeyBytes) {
      say(std::filesystem::path(*path).filename().string() + " is not a public key file (pp-client keygen).");
      return;
    }
    addKey(key, std::filesystem::path(*path).stem().string());
  });
}

void RegistrarScreen::removeKey(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
  const int index = indexArg(args);
  if (index < 0 || index >= static_cast<int>(keys_.size())) {
    return;
  }
  keys_.erase(keys_.begin() + index);
  keyBytes_.erase(keyBytes_.begin() + index);
  refresh();
  updateFee();
}

void RegistrarScreen::updateFee() {
  if (keyBytes_.empty()) {
    return;
  }
  auto fee = std::make_shared<uint64_t>(0);
  const auto keys = keyBytes_;
  const auto domain = domainOf(domain_);
  const std::string named = domain ? domain.value() : "";
  const auto minSignatures = parseAmount(minSignatures_);
  const uint32_t min = minSignatures && minSignatures.value() > 0 && minSignatures.value() <= keys.size()
                           ? static_cast<uint32_t>(minSignatures.value())
                           : 1;
  online_.withChain(
      [fee, keys, min, named](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        auto least = net::newAccountFee(chain.value(), keys, min, named);
        if (!least) {
          return least.error().message;
        }
        *fee = least.value();
        return "";
      },
      [this, fee](const std::string &error) {
        if (error.empty()) {
          fee_ = formatAmount(*fee);
          refresh();
        }
      });
}

void RegistrarScreen::makeAccount(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  auto id = parseAmount(id_);
  auto minSignatures = parseAmount(minSignatures_);
  auto amount = parseAmount(amount_);
  auto fee = parseAmount(fee_);
  if (!id || !net::isIssuedId(id.value())) {
    say("Type the account's number (from " + formatAmount(pp::AccountIds::ID_FIRST_ISSUED) + " to " +
        formatAmount(pp::AccountIds::ID_FIRST_USER - 1) + "), or pick a free one.");
    return;
  }
  if (keyBytes_.empty()) {
    say("Add the owner's key: their key card, or the .pub file pp-client keygen made.");
    return;
  }
  if (!minSignatures || minSignatures.value() == 0 || minSignatures.value() > keyBytes_.size()) {
    say("Signatures needed: between 1 and the keys listed.");
    return;
  }
  if (!amount) {
    say("Type what the account starts with (0 is allowed).");
    return;
  }
  if (!fee) {
    say("Type the fee: " + fee.error().message + ".");
    return;
  }
  auto domain = domainOf(domain_);
  if (!domain) {
    say(domain.error().message + " (its token's symbol), or none.");
    return;
  }
  const std::string named = domain.value();
  const uint64_t idValue = id.value();
  const auto keys = keyBytes_;
  const auto min = static_cast<uint32_t>(minSignatures.value());
  const uint64_t amountValue = amount.value();
  const uint64_t feeValue = fee.value();
  make(
      [=](net::ChainLink &link, const net::ChainInfo &chain, int64_t validFor,
          const std::string &notes) -> pp::Roe<op::Operation> {
        if (link.account(idValue)) {
          return pp::Error("Account " + formatAmount(idValue) + " exists already");
        }
        if (!named.empty()) {
          if (auto free = checkFree(link, named); !free) {
            return free.error();
          }
        }
        auto registrar = link.signersOf(pp::AccountIds::ID_REGISTRAR);
        if (!registrar) {
          return registrar.error();
        }
        return net::makeIssuedAccount(chain, registrar.value(), idValue, keys, min, amountValue, feeValue,
                                      {nowSeconds(), validFor, notes}, named);
      },
      "new-account-" + std::to_string(idValue) + ".json");
}

void RegistrarScreen::makeAssignment(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  auto id = parseAmount(assignId_);
  if (!id || !net::isIssuedId(id.value())) {
    say("Type the issued account's number.");
    return;
  }
  auto domain = domainOf(assignDomain_);
  if (!domain || domain.value().empty()) {
    say("Type its domain: 4 to 7 letters A-Z (its token's symbol, never changed).");
    return;
  }
  const uint64_t idValue = id.value();
  const std::string named = domain.value();
  make(
      [=](net::ChainLink &link, const net::ChainInfo &chain, int64_t validFor,
          const std::string &notes) -> pp::Roe<op::Operation> {
        auto account = link.account(idValue);
        if (!account) {
          return pp::Error("Account " + formatAmount(idValue) + ": " + account.error().message);
        }
        if (auto free = checkFree(link, named); !free) {
          return free.error();
        }
        auto registrar = link.signersOf(pp::AccountIds::ID_REGISTRAR);
        if (!registrar) {
          return registrar.error();
        }
        return net::makeDomainAssignment(chain, registrar.value(), idValue, account.value(), named,
                                         {nowSeconds(), validFor, notes});
      },
      "domain-" + named + ".json");
}

} // namespace apps::online
