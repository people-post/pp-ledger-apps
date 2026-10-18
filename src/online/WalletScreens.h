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
  /** The account the person chose to send from or evacuate (Accounts). */
  std::optional<uint64_t> selected;

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
  AccountsScreen(Online &online, ui_host::Session &session, WalletDesk &desk)
      : online_(online), session_(session), desk_(desk) {}
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
  void sendFrom(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void evacuate(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void choose(const ui::VariantList &args, const std::string &screen);
  void read();
  void say(const std::string &error, const std::string &notice = "");

  Online &online_;
  ui_host::Session &session_;
  WalletDesk &desk_;
  std::string tip_;
  std::vector<Row> rows_;
  std::string newAccount_;
  std::string newLabel_;
  std::string error_;
  std::string notice_;
  bool busy_{false};
};

/**
 * What Send and Evacuate share: an operation built from the proven account, shown as what it does, then
 * signed with the vault's keys and sent — when the vault holds enough of them — or saved as an
 * operation file for the account's other holders.
 */
class WalletAction : public ui_host::Screen {
public:
  WalletAction(Online &online, ui_host::Session &session, WalletDesk &desk,
               std::function<void(const std::string &)> made)
      : online_(online), session_(session), desk_(desk), made_(std::move(made)) {}

  struct Line {
    std::string label;
    std::string value;
  };

protected:
  /** Bind what every action shows: from, lines, signers, can_send, has_pending, error, notice, busy. */
  void bindAction(ui::DataModelConstructor &model);
  /** Show `operation` for the person to send or save. */
  void present(pp::operation::Operation operation);
  void clearPending();
  void say(const std::string &error, const std::string &notice = "");
  /** After it is in a block: read the account again, proven. */
  virtual void afterSent() {}
  /** The account chosen (Accounts), or say so; none when none. */
  std::optional<uint64_t> fromAccount();

  Online &online_;
  ui_host::Session &session_;
  WalletDesk &desk_;
  std::string from_;
  std::vector<Line> lines_;
  std::string signers_;
  bool canSend_{false};
  bool hasPending_{false};
  std::string error_;
  std::string notice_;
  bool busy_{false};

private:
  void sendIt(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void saveFile(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void cancel(ui::DataModelHandle, ui::Event &, const ui::VariantList &);

  std::function<void(const std::string &)> made_;
  std::optional<pp::operation::Operation> pending_;
};

class WalletSendScreen : public WalletAction {
public:
  using WalletAction::WalletAction;
  void willShow() override;

protected:
  std::string modelName() const override { return "wsend"; }
  std::string viewName() const override { return "wallet_send.rml"; }
  void bind(ui::DataModelConstructor &model) override;
  void afterSent() override;

private:
  void prepare(ui::DataModelHandle, ui::Event &, const ui::VariantList &);

  std::string balance_;
  std::string to_;
  std::string amount_;
  std::string token_;
};

class EvacuateScreen : public WalletAction {
public:
  using WalletAction::WalletAction;
  void willShow() override;

protected:
  std::string modelName() const override { return "wevac"; }
  std::string viewName() const override { return "wallet_evacuate.rml"; }
  void bind(ui::DataModelConstructor &model) override;
  void afterSent() override;

private:
  void replaceKeys(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void moveAll(ui::DataModelHandle, ui::Event &, const ui::VariantList &);

  std::string holds_;
  std::string moveTo_;
};

} // namespace apps::online
