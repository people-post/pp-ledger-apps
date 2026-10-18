#pragma once
// The wallet's screens (projects/wallet-app/DESIGN.md § What it does): the network it pinned and what
// each node says; the person's accounts, each proven from the pinned genesis hash (WL003).

#include "online/Online.h"
#include "net/WalletChain.h"
#include "core/WalletBook.h"
#include "ui/Screen.h"

#include <memory>
#include <optional>
#include <vector>

namespace apps::online {

/** The wallet's records and its verifier, shared by its screens. */
class WalletDesk {
public:
  WalletDesk(Online &online, ui_host::Session &session) : online_(online), session_(session) {}

  /** Read the records (after the vault unlocks); "" or what is wrong. */
  std::string load();
  pp::Roe<void> save();
  wallet::WalletBook &book() { return book_; }
  /** Pin `card`: the verifier starts from it, the node it talks to is its first. */
  pp::Roe<void> pin(const wallet::NetworkCard &card);
  /** The verifier for the pinned card; none until one is pinned. Use it inside Online::withChain only. */
  std::shared_ptr<net::Verifier> verifier() const { return verifier_; }

private:
  Online &online_;
  ui_host::Session &session_;
  wallet::WalletBook book_;
  std::shared_ptr<net::Verifier> verifier_;
};

class WalletNetworkScreen : public ui_host::Screen {
public:
  WalletNetworkScreen(Online &online, WalletDesk &desk) : online_(online), desk_(desk) {}
  void willShow() override;

  struct NodeRow {
    std::string address;
    std::string state;
    bool current{false};
  };

protected:
  std::string modelName() const override { return "wnet"; }
  std::string viewName() const override { return "wallet_network.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void importCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void pinTyped(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void checkAll(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void useNode(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void pin(const wallet::NetworkCard &card, const std::string &how);
  void say(const std::string &error, const std::string &notice = "");

  Online &online_;
  WalletDesk &desk_;
  bool pinned_{false};
  std::string network_;
  std::string genesis_;
  std::vector<NodeRow> nodes_;
  std::string typedNetwork_;
  std::string typedGenesis_;
  std::string typedNodes_;
  std::string error_;
  std::string notice_;
  bool busy_{false};
};

class AccountsScreen : public ui_host::Screen {
public:
  AccountsScreen(Online &online, WalletDesk &desk) : online_(online), desk_(desk) {}
  void willShow() override;

  struct Row {
    std::string label;
    std::string account;
    std::string balance;
    std::string tokens;
    std::string keys;
    std::string renewal;
    std::string state;
    bool proven{false};
  };

protected:
  std::string modelName() const override { return "accounts"; }
  std::string viewName() const override { return "accounts.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void add(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void remove(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void refreshAll(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void read();
  void say(const std::string &error, const std::string &notice = "");

  Online &online_;
  WalletDesk &desk_;
  std::string tip_;
  std::vector<Row> rows_;
  std::string newAccount_;
  std::string newLabel_;
  std::string error_;
  std::string notice_;
  bool busy_{false};
};

} // namespace apps::online
