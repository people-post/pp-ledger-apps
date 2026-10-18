#pragma once
// The wallet's screens (projects/wallet-app/DESIGN.md § What it does): the network it pinned and what
// each node says; the person's accounts, each proven from the pinned genesis hash (WL003).

#include "online/Online.h"
#include "online/RecoveryParts.h"
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
    /** Its recovery set, or "" (none). */
    std::string recovery;
    /** A recovery announced: what and when, for the owner to cancel if not theirs. */
    std::string alert;
    /** Its domain (its token's symbol), proven; "" when none. */
    std::string domain;
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
  void recovery(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
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
  /** Whose keys sign, in what the action says of them ("the account's", "its recovery set's"). */
  std::string signersAre_{"the account's"};

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

/**
 * An account's way back (pp-ledger projects/account-recovery): its recovery set stands above its keys.
 * For the chosen account: its recovery, proven — frozen while an announcement waits — and naming its
 * first set (its keys sign). For whoever holds an account's recovery keys: announcing its new keys,
 * withdrawing that, changing or removing the set (the set signs).
 */
class AccountRecoveryScreen : public WalletAction {
public:
  using WalletAction::WalletAction;
  void willShow() override;

protected:
  std::string modelName() const override { return "wrecov"; }
  std::string viewName() const override { return "wallet_recovery.rml"; }
  void bind(ui::DataModelConstructor &model) override;
  void afterSent() override;

private:
  // The chosen account's first set.
  void addSetCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void dropSetKey(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void setIt(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  // The recovery set's acts on account `target_`.
  void addNewCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void makeNewKey(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void dropNewKey(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void announce(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void withdraw(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void addChangeCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void dropChangeKey(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void change(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void remove(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  /**
   * Prove account `id` and read its whole state and the chain, then `then` (on the UI thread) with them;
   * or say why not.
   */
  void withProven(uint64_t id, std::function<void(const net::VerifiedAccount &, const pp::UserAccount &,
                                                  const net::ChainInfo &)> then);
  /**
   * The recovery set's act on account `target_`: `build` from its proven state, shown for the set's
   * holders to sign.
   */
  void bySet(const std::string &making,
             std::function<pp::Roe<pp::operation::Operation>(uint64_t, const pp::UserAccount &, const net::ChainInfo &)>
                 build,
             std::function<std::string(const net::VerifiedAccount &, const net::ChainInfo &)> notice);
  /** A key card's key into `into`. */
  void addCard(ChosenKeys &into);
  void read();

  /** The account chosen under Accounts (none: only acting as a recovery set is offered). */
  bool hasChosen_{false};
  std::string chosen_;
  /** The chosen account's recovery now, proven; "" until read. */
  std::string now_;
  std::string announced_;
  bool hasSet_{false};
  bool isAnnounced_{false};
  ChosenKeys setKeys_;
  std::string setNeeded_{"1"};
  std::string days_{"7"};
  /** Acting as an account's recovery set: the account, its new keys, a new set. */
  std::string target_;
  ChosenKeys newKeys_;
  std::string newNeeded_{"1"};
  ChosenKeys changeKeys_;
  std::string changeNeeded_{"1"};
  std::string changeDays_{"7"};
  /** The operation shown is the set's, on this account (not the chosen account's own). */
  std::optional<uint64_t> actedOn_;
};

} // namespace apps::online
