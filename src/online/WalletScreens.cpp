#include "online/WalletScreens.h"

#include "net/TokenOperations.h"
#include "core/AtomicFile.h"
#include "core/Amounts.h"
#include "client/AccountIds.h"
#include "lib/common/Utilities.h"

#include <algorithm>
#include <chrono>
#include <sstream>

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

} // namespace apps::online
