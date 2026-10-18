#include "online/WalletScreens.h"

#include "net/TokenOperations.h"
#include "core/Operations.h"
#include "core/Roles.h"
#include "lib/common/BinaryPack.hpp"
#include "core/AtomicFile.h"
#include "core/Amounts.h"
#include "client/AccountIds.h"
#include "lib/common/Utilities.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <cctype>

namespace apps::online {

namespace {

const std::vector<ui_host::FileFilter> kCards = {{"Network cards", "json"}};

int indexArg(const ui::VariantList &args) { return args.empty() ? -1 : args[0].Get<int>(-1); }

std::string trimmed(const std::string &text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

int64_t now() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string ago(int64_t timestamp) {
  const int64_t seconds = std::max<int64_t>(0, now() - timestamp);
  if (seconds < 120) {
    return std::to_string(seconds) + " s ago";
  }
  if (seconds < 7200) {
    return std::to_string(seconds / 60) + " min ago";
  }
  if (seconds < 172800) {
    return std::to_string(seconds / 3600) + " h ago";
  }
  return std::to_string(seconds / 86400) + " days ago";
}

std::string tipText(const net::VerifiedTip &tip) {
  return "block " + formatAmount(tip.height) + ", signed by the beacon (generation " + std::to_string(tip.generation) +
         "), " + ago(tip.timestamp);
}

uint64_t native(const pp::Client::Wallet &wallet) {
  const auto it = wallet.mBalances.find(pp::AccountIds::ID_GENESIS);
  return it == wallet.mBalances.end() || it->second < 0 ? 0 : static_cast<uint64_t>(it->second);
}

} // namespace

// ---- WalletDesk ----

std::string WalletDesk::load() {
  auto book = wallet::WalletBook::load(session_.vault);
  book_ = book ? std::move(book.value()) : wallet::WalletBook{};
  verifier_ = book_.card ? std::make_shared<net::Verifier>(*book_.card) : nullptr;
  return book ? "" : book.error().message;
}

pp::Roe<void> WalletDesk::save() { return book_.save(session_.vault); }

pp::Roe<void> WalletDesk::pin(const wallet::NetworkCard &card) {
  if (card.nodes.empty()) {
    return pp::Error("The card names no node to ask");
  }
  book_.card = card;
  if (auto saved = save(); !saved) {
    return saved;
  }
  verifier_ = std::make_shared<net::Verifier>(card);
  return online_.saveAddress(card.nodes.front());
}

// ---- Network ----

void WalletNetworkScreen::bind(ui::DataModelConstructor &model) {
  if (auto row = model.RegisterStruct<NodeRow>()) {
    row.RegisterMember("address", &NodeRow::address);
    row.RegisterMember("state", &NodeRow::state);
    row.RegisterMember("current", &NodeRow::current);
  }
  model.RegisterArray<std::vector<NodeRow>>();
  model.Bind("pinned", &pinned_);
  model.Bind("network", &network_);
  model.Bind("genesis", &genesis_);
  model.Bind("nodes", &nodes_);
  model.Bind("typed_network", &typedNetwork_);
  model.Bind("typed_genesis", &typedGenesis_);
  model.Bind("typed_nodes", &typedNodes_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
  model.BindEventCallback("wnet_import", &WalletNetworkScreen::importCard, this);
  model.BindEventCallback("wnet_pin", &WalletNetworkScreen::pinTyped, this);
  model.BindEventCallback("wnet_check", &WalletNetworkScreen::checkAll, this);
  model.BindEventCallback("wnet_use", &WalletNetworkScreen::useNode, this);
}

void WalletNetworkScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void WalletNetworkScreen::willShow() {
  const auto &card = desk_.book().card;
  pinned_ = card.has_value();
  nodes_.clear();
  if (!card) {
    network_.clear();
    genesis_.clear();
    say("", "Pin your network: import the network card its operators publish, or type it. Check block 0's hash "
            "against a second source you trust — everything the wallet shows is checked back to it.");
    return;
  }
  network_ = card->networkId;
  genesis_ = wallet::hashForPeople(card->genesisHash);
  for (const auto &node : card->nodes) {
    nodes_.push_back({node, "not asked yet", node == online_.address()});
  }
  say("");
}

void WalletNetworkScreen::pin(const wallet::NetworkCard &card, const std::string &how) {
  if (auto pinned = desk_.pin(card); !pinned) {
    say(pinned.error().message + ".");
    return;
  }
  willShow();
  say("", "Pinned " + how + ". Block 0's hash: " + wallet::hashForPeople(card.genesisHash) +
              " — check it against a second source. Now check the nodes.");
}

void WalletNetworkScreen::importCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  app_->chooseFileToOpen(kCards, [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    auto text = readFile(*path);
    auto card = text ? wallet::networkCardFromJson(text.value()) : pp::Roe<wallet::NetworkCard>(text.error());
    if (!card) {
      say(card.error().message + ".");
      return;
    }
    pin(card.value(), "from the card");
  });
}

void WalletNetworkScreen::pinTyped(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  wallet::NetworkCard card;
  card.networkId = trimmed(typedNetwork_);
  std::string hex = trimmed(typedGenesis_);
  hex.erase(std::remove_if(hex.begin(), hex.end(), [](unsigned char c) { return std::isspace(c); }), hex.end());
  if (hex.rfind("0x", 0) == 0) {
    hex = hex.substr(2);
  }
  card.genesisHash = pp::utl::hexDecode(hex);
  std::istringstream nodes(typedNodes_);
  for (std::string node; std::getline(nodes, node, ',');) {
    if (!trimmed(node).empty()) {
      card.nodes.push_back(trimmed(node));
    }
  }
  if (card.networkId.empty() || card.genesisHash.size() != 32 || card.nodes.empty()) {
    say("Type the network's id, block 0's hash (64 hex digits) and at least one node's address.");
    return;
  }
  pin(card, "as typed");
}

void WalletNetworkScreen::checkAll(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto &card = desk_.book().card;
  if (!card || busy_) {
    return;
  }
  busy_ = true;
  say("", "Asking each node, and checking what it says back to block 0…");
  auto views = std::make_shared<std::vector<net::NodeView>>();
  const wallet::NetworkCard pinned = *card;
  app_->runInBackground([views, pinned] { *views = net::checkNodes(pinned); },
                        [this, views] {
                          busy_ = false;
                          uint64_t highest = 0;
                          for (const auto &v : *views) {
                            if (v.tip) {
                              highest = std::max(highest, v.tip->height);
                            }
                          }
                          nodes_.clear();
                          size_t good = 0;
                          for (const auto &v : *views) {
                            std::string state;
                            if (v.tip) {
                              ++good;
                              state = "verified: " + tipText(*v.tip);
                              if (highest - v.tip->height > net::kBehindBlocks) {
                                state += " — BEHIND the others by " + formatAmount(highest - v.tip->height) + " blocks";
                              }
                            } else {
                              state = "not verified: " + v.error;
                            }
                            nodes_.push_back({v.address, state, v.address == online_.address()});
                          }
                          say(good == 0 ? "No node's answer could be verified." : "",
                              std::to_string(good) + " of " + std::to_string(views->size()) +
                                  " nodes verified back to block 0.");
                        });
}

void WalletNetworkScreen::useNode(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
  const int index = indexArg(args);
  if (index < 0 || index >= static_cast<int>(nodes_.size())) {
    return;
  }
  if (auto saved = online_.saveAddress(nodes_[static_cast<size_t>(index)].address); !saved) {
    say(saved.error().message);
    return;
  }
  for (size_t i = 0; i < nodes_.size(); ++i) {
    nodes_[i].current = static_cast<int>(i) == index;
  }
  say("", "The wallet asks this node from now on.");
}

// ---- Accounts ----

void AccountsScreen::bind(ui::DataModelConstructor &model) {
  if (auto row = model.RegisterStruct<Row>()) {
    row.RegisterMember("label", &Row::label);
    row.RegisterMember("account", &Row::account);
    row.RegisterMember("balance", &Row::balance);
    row.RegisterMember("tokens", &Row::tokens);
    row.RegisterMember("keys", &Row::keys);
    row.RegisterMember("renewal", &Row::renewal);
    row.RegisterMember("state", &Row::state);
    row.RegisterMember("proven", &Row::proven);
  }
  model.RegisterArray<std::vector<Row>>();
  model.Bind("tip", &tip_);
  model.Bind("rows", &rows_);
  model.Bind("new_account", &newAccount_);
  model.Bind("new_label", &newLabel_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
  model.BindEventCallback("accounts_add", &AccountsScreen::add, this);
  model.BindEventCallback("accounts_remove", &AccountsScreen::remove, this);
  model.BindEventCallback("accounts_refresh", &AccountsScreen::refreshAll, this);
  model.BindEventCallback("accounts_send", &AccountsScreen::sendFrom, this);
  model.BindEventCallback("accounts_evacuate", &AccountsScreen::evacuate, this);
}

void AccountsScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void AccountsScreen::willShow() {
  rows_.clear();
  for (const auto &a : desk_.book().accounts) {
    rows_.push_back({a.label, formatAmount(a.id), "…", "", "", "", "reading…", false});
  }
  if (!desk_.verifier()) {
    tip_ = "no network pinned";
    say("Pin your network first (Network): the wallet shows nothing it cannot check.");
    return;
  }
  read();
}

void AccountsScreen::read() {
  auto verifier = desk_.verifier();
  if (!verifier || busy_) {
    return;
  }
  busy_ = true;
  say("", rows_.empty() ? "" : "Proving each account from block 0…");
  struct Read {
    std::optional<net::VerifiedAccount> account;
    std::string error;
    std::string renewal;
  };
  auto results = std::make_shared<std::vector<Read>>();
  auto tip = std::make_shared<std::optional<net::VerifiedTip>>();
  auto tipError = std::make_shared<std::string>();
  std::vector<uint64_t> ids;
  for (const auto &a : desk_.book().accounts) {
    ids.push_back(a.id);
  }
  online_.withChain(
      [verifier, ids, results, tip, tipError](net::ChainLink &link) -> std::string {
        auto verifiedTip = verifier->tip(link);
        if (verifiedTip) {
          *tip = verifiedTip.value();
        } else {
          *tipError = verifiedTip.error().message;
        }
        for (uint64_t id : ids) {
          Read r;
          auto account = verifier->account(link, id);
          if (account) {
            r.account = account.value();
            // The renewal fee depends on the config and the account's attachment: the node's word.
            auto chain = link.info();
            auto state = link.account(id);
            if (chain && state && account.value().exists) {
              if (auto fee = net::renewalFee(chain.value(), id, state.value())) {
                r.renewal = "about " + formatAmount(fee.value()) + " a renewal (the node's word)";
              }
            }
          } else {
            r.error = account.error().message;
          }
          results->push_back(std::move(r));
        }
        return "";
      },
      [this, results, tip, tipError](const std::string &error) {
        busy_ = false;
        tip_ = *tip ? tipText(**tip) : "not verified: " + (tipError->empty() ? error : *tipError);
        const auto &accounts = desk_.book().accounts;
        rows_.clear();
        for (size_t i = 0; i < accounts.size(); ++i) {
          Row row{accounts[i].label, formatAmount(accounts[i].id), "", "", "", "", "", false};
          const Read *r = i < results->size() ? &(*results)[i] : nullptr;
          if (!r || !r->account) {
            row.balance = "unknown";
            row.state = "not verified: " + (r ? r->error : error);
          } else if (!r->account->exists) {
            row.balance = "—";
            row.state = "proven absent at block " + formatAmount(r->account->provenAtBlock) +
                        ": no such account (yet, or any more)";
            row.proven = true;
          } else {
            const auto &w = r->account->wallet;
            row.balance = formatAmount(native(w));
            for (const auto &[token, amount] : w.mBalances) {
              if (token != pp::AccountIds::ID_GENESIS && amount != 0) {
                row.tokens += (row.tokens.empty() ? "" : ", ") + std::string(amount < 0 ? "-" : "") +
                              formatAmount(static_cast<uint64_t>(amount < 0 ? -amount : amount)) + " of token " +
                              formatAmount(token);
              }
            }
            row.keys = std::to_string(w.minSignatures) + " of " + std::to_string(w.publicKeys.size()) + " keys";
            row.renewal = r->renewal;
            row.state = "proven at block " + formatAmount(r->account->provenAtBlock);
            row.proven = true;
          }
          rows_.push_back(std::move(row));
        }
        say(error.empty() ? "" : error + ".");
      });
}

void AccountsScreen::add(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  auto id = parseAmount(newAccount_);
  if (!id) {
    say("Type the account's number.");
    return;
  }
  auto &accounts = desk_.book().accounts;
  if (std::any_of(accounts.begin(), accounts.end(), [&](const wallet::FollowedAccount &a) { return a.id == id.value(); })) {
    say("That account is listed already.");
    return;
  }
  accounts.push_back({id.value(), trimmed(newLabel_)});
  if (auto saved = desk_.save(); !saved) {
    say(saved.error().message);
    return;
  }
  newAccount_.clear();
  newLabel_.clear();
  willShow();
}

void AccountsScreen::remove(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
  const int index = indexArg(args);
  auto &accounts = desk_.book().accounts;
  if (index < 0 || index >= static_cast<int>(accounts.size())) {
    return;
  }
  accounts.erase(accounts.begin() + index);
  if (auto saved = desk_.save(); !saved) {
    say(saved.error().message);
    return;
  }
  willShow();
}

void AccountsScreen::refreshAll(ui::DataModelHandle, ui::Event &, const ui::VariantList &) { read(); }

void AccountsScreen::choose(const ui::VariantList &args, const std::string &screen) {
  const int index = indexArg(args);
  const auto &accounts = desk_.book().accounts;
  if (index < 0 || index >= static_cast<int>(accounts.size())) {
    return;
  }
  desk_.selected = accounts[static_cast<size_t>(index)].id;
  session_.navigate(screen);
}

void AccountsScreen::sendFrom(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) { choose(args, "send"); }

void AccountsScreen::evacuate(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
  choose(args, "evacuate");
}

// ---- What Send and Evacuate share ----

void WalletAction::bindAction(ui::DataModelConstructor &model) {
  if (auto line = model.RegisterStruct<Line>()) {
    line.RegisterMember("label", &Line::label);
    line.RegisterMember("value", &Line::value);
  }
  model.RegisterArray<std::vector<Line>>();
  model.Bind("from", &from_);
  model.Bind("lines", &lines_);
  model.Bind("signers", &signers_);
  model.Bind("can_send", &canSend_);
  model.Bind("has_pending", &hasPending_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
  model.BindEventCallback("action_send", &WalletAction::sendIt, this);
  model.BindEventCallback("action_file", &WalletAction::saveFile, this);
  model.BindEventCallback("action_cancel", &WalletAction::cancel, this);
}

void WalletAction::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

std::optional<uint64_t> WalletAction::fromAccount() {
  if (!desk_.selected) {
    from_ = "none chosen";
    say("Choose the account under Accounts.");
    return std::nullopt;
  }
  from_ = formatAmount(*desk_.selected);
  for (const auto &a : desk_.book().accounts) {
    if (a.id == *desk_.selected && !a.label.empty()) {
      from_ = a.label + " (" + from_ + ")";
    }
  }
  return desk_.selected;
}

void WalletAction::clearPending() {
  pending_.reset();
  hasPending_ = false;
  canSend_ = false;
  lines_.clear();
  signers_.clear();
}

void WalletAction::present(pp::operation::Operation operation) {
  lines_.clear();
  if (auto summary = pp::operation::summarize(operation)) {
    lines_.push_back({"What", summary.value().title});
    for (const auto &[label, value] : summary.value().lines) {
      lines_.push_back({label, value});
    }
  }
  const auto keys = session_.vault.keys();
  const auto mine = apps::keysToSign(operation, keys ? keys.value() : std::vector<KeyInfo>{});
  const uint32_t needed = operation.signers.minSignatures;
  canSend_ = mine.size() >= needed;
  signers_ = "It needs " + std::to_string(needed) + " of the account's " +
             std::to_string(operation.signers.publicKeys.size()) + " keys; this wallet holds " +
             std::to_string(mine.size()) + "." +
             (canSend_ ? "" : " Save it as an operation file for the account's other holders to sign.");
  pending_ = std::move(operation);
  hasPending_ = true;
  refresh();
}

void WalletAction::sendIt(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (!pending_ || !canSend_ || busy_) {
    return;
  }
  pp::operation::Operation operation = *pending_;
  const auto keys = session_.vault.keys();
  const auto mine = apps::keysToSign(operation, keys ? keys.value() : std::vector<KeyInfo>{});
  for (size_t i = 0; i < mine.size() && i < operation.signers.minSignatures; ++i) {
    if (auto signedIt = apps::signWithVault(session_.vault, mine[i].id, operation); !signedIt) {
      say(signedIt.error().message);
      return;
    }
  }
  busy_ = true;
  say("", "Sending…");
  session_.submit(operation, [this](const ui_host::Session::SubmitProgress &progress) {
    if (progress.failed) {
      busy_ = false;
      say(progress.status);
      return;
    }
    if (!progress.done) {
      say("", progress.status);
      return;
    }
    busy_ = false;
    clearPending();
    say("", "In a block. Proving the account again…");
    afterSent();
  });
}

void WalletAction::saveFile(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (!pending_) {
    return;
  }
  const pp::operation::Operation operation = *pending_;
  app_->chooseFileToSave({{"Operation files", "json"}}, "operation.json", [this, operation](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    if (auto saved = pp::operation::save(operation, *path); !saved) {
      say(saved.error().message);
      return;
    }
    clearPending();
    say("", "Saved: sign it, and hand a copy to each of the account's other holders.");
    made_(*path);
  });
}

void WalletAction::cancel(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  clearPending();
  say("", "Nothing sent.");
}

// ---- Send ----

void WalletSendScreen::bind(ui::DataModelConstructor &model) {
  bindAction(model);
  model.Bind("balance", &balance_);
  model.Bind("to", &to_);
  model.Bind("amount", &amount_);
  model.Bind("token", &token_);
  model.BindEventCallback("send_prepare", &WalletSendScreen::prepare, this);
}

void WalletSendScreen::willShow() {
  clearPending();
  balance_.clear();
  if (fromAccount()) {
    say("", "Who it goes to, and how much: the wallet checks both against the chain, proven, before you send.");
  }
}

void WalletSendScreen::prepare(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto from = fromAccount();
  auto verifier = desk_.verifier();
  if (!from || !verifier || busy_) {
    return;
  }
  auto to = parseAmount(to_);
  auto amount = parseAmount(amount_);
  uint64_t token = pp::AccountIds::ID_GENESIS;
  if (!trimmed(token_).empty()) {
    auto typed = parseAmount(token_);
    if (!typed) {
      say("Type the token's number, or leave it empty for the native token.");
      return;
    }
    token = typed.value();
  }
  if (!to || !amount || amount.value() == 0) {
    say("Type the account it goes to and an amount above 0.");
    return;
  }
  busy_ = true;
  say("", "Proving both accounts, and making the transfer…");
  auto made = std::make_shared<pp::operation::Operation>();
  auto balance = std::make_shared<std::string>();
  const uint64_t fromId = *from;
  const uint64_t toId = to.value();
  const uint64_t value = amount.value();
  online_.withChain(
      [verifier, made, balance, fromId, toId, value, token](net::ChainLink &link) -> std::string {
        auto source = verifier->account(link, fromId);
        if (!source) {
          return "Your account could not be proven: " + source.error().message;
        }
        if (!source.value().exists) {
          return "Your account does not exist (proven)";
        }
        *balance = formatAmount(native(source.value().wallet));
        auto target = verifier->account(link, toId);
        if (!target) {
          return "The account it goes to could not be proven: " + target.error().message;
        }
        if (!target.value().exists) {
          return "Account " + formatAmount(toId) + " does not exist (proven): check the number";
        }
        auto state = net::provenState(link, source.value());
        auto chain = link.info();
        if (!state || !chain) {
          return state ? chain.error().message : state.error().message;
        }
        pp::Ledger::TxDefault probe;
        probe.fromWalletId = fromId;
        pp::Ledger::Record record;
        record.type = pp::Ledger::T_DEFAULT;
        record.data = pp::utl::binaryPack(probe);
        auto fee = link.minimumFee(record);
        if (!fee) {
          return fee.error().message;
        }
        auto op = net::makeSend(chain.value(), net::signersOf(source.value()), state.value(), toId, token, value,
                                fee.value(), {now(), 3600, ""});
        if (!op) {
          return op.error().message;
        }
        *made = std::move(op.value());
        return "";
      },
      [this, made, balance](const std::string &error) {
        busy_ = false;
        balance_ = *balance;
        if (!error.empty()) {
          say(error + ".");
          return;
        }
        present(*made);
        say("", "Check it. It is valid for an hour.");
      });
}

void WalletSendScreen::afterSent() {
  const auto from = fromAccount();
  auto verifier = desk_.verifier();
  if (!from || !verifier) {
    return;
  }
  auto proven = std::make_shared<std::optional<net::VerifiedAccount>>();
  const uint64_t id = *from;
  online_.withChain(
      [verifier, proven, id](net::ChainLink &link) -> std::string {
        auto account = verifier->account(link, id);
        if (!account) {
          return account.error().message;
        }
        *proven = account.value();
        return "";
      },
      [this, proven](const std::string &error) {
        if (!error.empty() || !*proven) {
          say("", "Sent and in a block. Reading it back, proven, failed for now: " + error + ".");
          return;
        }
        balance_ = formatAmount(native((*proven)->wallet));
        to_.clear();
        amount_.clear();
        say("", "Sent. Proven at block " + formatAmount((*proven)->provenAtBlock) + ": the account holds " + balance_ +
                    ".");
      });
}

// ---- Evacuate ----

void EvacuateScreen::bind(ui::DataModelConstructor &model) {
  bindAction(model);
  model.Bind("holds", &holds_);
  model.Bind("move_to", &moveTo_);
  model.BindEventCallback("evac_keys", &EvacuateScreen::replaceKeys, this);
  model.BindEventCallback("evac_move", &EvacuateScreen::moveAll, this);
}

void EvacuateScreen::willShow() {
  clearPending();
  holds_.clear();
  if (fromAccount()) {
    say("", "If a key of this account may have leaked — through a plugin, a lost device — act before anyone else "
            "does: replace its keys, or move everything it holds to another account of yours.");
  }
}

void EvacuateScreen::replaceKeys(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto from = fromAccount();
  auto verifier = desk_.verifier();
  if (!from || !verifier || busy_) {
    return;
  }
  // The new key, made here: whoever has the old ones never sees it.
  auto key = session_.vault.generateKey("account " + std::to_string(*from) + ", new key (evacuation)", Role::Owner);
  if (!key) {
    say(key.error().message);
    return;
  }
  busy_ = true;
  say("", "Proving the account, and making the key replacement…");
  auto made = std::make_shared<pp::operation::Operation>();
  const uint64_t id = *from;
  const std::string newKey = key.value().publicKey;
  online_.withChain(
      [verifier, made, id, newKey](net::ChainLink &link) -> std::string {
        auto account = verifier->account(link, id);
        if (!account || !account.value().exists) {
          return account ? "The account does not exist (proven)" : account.error().message;
        }
        auto state = net::provenState(link, account.value());
        auto chain = link.info();
        if (!state || !chain) {
          return state ? chain.error().message : state.error().message;
        }
        const auto signers = net::signersOf(account.value());
        auto fee = net::keyReplacementFee(chain.value(), signers, state.value(), {newKey}, 1);
        if (!fee) {
          return fee.error().message;
        }
        auto op = net::makeKeyReplacement(chain.value(), signers, state.value(), {newKey}, 1, fee.value(),
                                          {now(), 3600, "evacuation: a new key"});
        if (!op) {
          return op.error().message;
        }
        *made = std::move(op.value());
        return "";
      },
      [this, made, newKey](const std::string &error) {
        busy_ = false;
        if (!error.empty()) {
          say(error + ". The new key stays in the vault, unused.");
          return;
        }
        present(*made);
        say("", "From now on only the new key (" + pp::operation::fingerprint(newKey) +
                    ", in this wallet) signs for the account. Back the vault up once it is done.");
      });
}

void EvacuateScreen::moveAll(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto from = fromAccount();
  auto verifier = desk_.verifier();
  if (!from || !verifier || busy_) {
    return;
  }
  auto to = parseAmount(moveTo_);
  if (!to || to.value() == *from) {
    say("Type the number of another account of yours, whose keys never were where the leak was.");
    return;
  }
  busy_ = true;
  say("", "Proving both accounts, and making the transfers…");
  auto made = std::make_shared<pp::operation::Operation>();
  const uint64_t id = *from;
  const uint64_t toId = to.value();
  online_.withChain(
      [verifier, made, id, toId](net::ChainLink &link) -> std::string {
        auto account = verifier->account(link, id);
        if (!account || !account.value().exists) {
          return account ? "The account does not exist (proven)" : account.error().message;
        }
        auto target = verifier->account(link, toId);
        if (!target || !target.value().exists) {
          return target ? "Account " + formatAmount(toId) + " does not exist (proven)" : target.error().message;
        }
        auto state = net::provenState(link, account.value());
        auto chain = link.info();
        if (!state || !chain) {
          return state ? chain.error().message : state.error().message;
        }
        auto op = net::makeMoveAll(chain.value(), net::signersOf(account.value()), state.value(), toId,
                                   {now(), 3600, "evacuation: everything moved"});
        if (!op) {
          return op.error().message;
        }
        *made = std::move(op.value());
        return "";
      },
      [this, made](const std::string &error) {
        busy_ = false;
        if (!error.empty()) {
          say(error + ".");
          return;
        }
        present(*made);
        say("", "Everything it holds goes, the native token last less the fees. With nothing left for its "
                "renewal, the account ends at its next one.");
      });
}

void EvacuateScreen::afterSent() {
  const auto from = fromAccount();
  auto verifier = desk_.verifier();
  if (!from || !verifier) {
    return;
  }
  auto proven = std::make_shared<std::optional<net::VerifiedAccount>>();
  const uint64_t id = *from;
  online_.withChain(
      [verifier, proven, id](net::ChainLink &link) -> std::string {
        auto account = verifier->account(link, id);
        if (!account) {
          return account.error().message;
        }
        *proven = account.value();
        return "";
      },
      [this, proven](const std::string &error) {
        if (!error.empty() || !*proven) {
          say("", "Done and in a block. Reading it back, proven, failed for now: " + error + ".");
          return;
        }
        const auto &w = (*proven)->wallet;
        holds_ = formatAmount(native(w)) + " native; " + std::to_string(w.minSignatures) + " of " +
                 std::to_string(w.publicKeys.size()) + " keys";
        moveTo_.clear();
        say("", "Done. Proven at block " + formatAmount((*proven)->provenAtBlock) + ": the account holds " + holds_ +
                    ". Back the vault up now.");
      });
}

} // namespace apps::online
