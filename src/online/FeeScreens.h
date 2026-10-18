#pragma once
// The fee app's screens (projects/fee-app/DESIGN.md § Screens): the rounds so far, the next round's
// period read from the chain, the relays' measures, the rules, the payouts made into one operation.
// What they share — the relays, the rules in force, the draft round, the rounds — is the FeeDesk,
// kept in the vault (fees.json).

#include "online/OperationForm.h"
#include "core/FeeRounds.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace apps::online {

/** The fee app's records, loaded when the vault unlocks and saved on each change. */
class FeeDesk {
public:
  explicit FeeDesk(ui_host::Session &session) : session_(session) {}

  /** Read the records (after the vault unlocks); "" or what is wrong. */
  std::string load();
  pp::Roe<void> save();
  fees::FeeBook &book() { return book_; }
  /** The rules in force, or a starting point when none are kept yet. */
  fees::Rules rules() const;
  /** The next round's number, and the block it starts at (where the last ended, or block 1). */
  uint64_t nextNumber() const;
  uint64_t nextStart() const;

private:
  ui_host::Session &session_;
  fees::FeeBook book_;
};

/** A line of a table: label and value. */
struct FeeFact {
  std::string label;
  std::string value;
};

class RoundsScreen : public ui_host::Screen {
public:
  RoundsScreen(Online &online, ui_host::Session &session, FeeDesk &desk)
      : online_(online), session_(session), desk_(desk) {}
  void willShow() override;

  struct Row {
    std::string number;
    std::string blocks;
    std::string paid;
    std::string digest;
  };

protected:
  std::string modelName() const override { return "rounds"; }
  std::string viewName() const override { return "rounds.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void startNext(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void importRound(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void exportRound(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void say(const std::string &error, const std::string &notice = "");

  Online &online_;
  ui_host::Session &session_;
  FeeDesk &desk_;
  std::string balance_;
  std::vector<Row> rows_;
  std::string error_;
  std::string notice_;
};

class PeriodScreen : public ui_host::Screen {
public:
  PeriodScreen(Online &online, FeeDesk &desk) : online_(online), desk_(desk) {}
  void willShow() override;

  struct MinerRow {
    std::string account;
    std::string blocks;
    std::string fees;
    std::string misses;
    std::string registered;
  };

protected:
  std::string modelName() const override { return "period"; }
  std::string viewName() const override { return "period.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void read(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void show(const fees::Report &report);
  void say(const std::string &error, const std::string &notice = "");

  Online &online_;
  FeeDesk &desk_;
  std::string number_;
  std::string from_;
  std::string endsAt_;
  std::vector<FeeFact> facts_;
  std::vector<MinerRow> miners_;
  std::string error_;
  std::string notice_;
  bool busy_{false};
};

class RelaysScreen : public ui_host::Screen {
public:
  explicit RelaysScreen(FeeDesk &desk) : desk_(desk) {}
  void willShow() override;

  struct RelayRow {
    std::string name;
    std::string peerId;
    std::string account;
    std::string uptime;
  };

protected:
  std::string modelName() const override { return "relays"; }
  std::string viewName() const override { return "relays.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void add(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void remove(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void importCsv(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void keep(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  /** The rows into the relay list and the draft's measures; false (said) when a row is wrong. */
  bool apply();
  void say(const std::string &error, const std::string &notice = "");

  FeeDesk &desk_;
  std::vector<RelayRow> rows_;
  std::string newName_;
  std::string newPeerId_;
  std::string newAccount_;
  std::string source_;
  std::string enteredBy_;
  bool hasDraft_{false};
  std::string error_;
  std::string notice_;
};

/** A payout as tables show it. */
struct PayoutRow {
  std::string account;
  std::string who;
  std::string amount;
  std::string last;
};

class RulesScreen : public ui_host::Screen {
public:
  RulesScreen(Online &online, FeeDesk &desk) : online_(online), desk_(desk) {}
  void willShow() override;

protected:
  std::string modelName() const override { return "rules"; }
  std::string viewName() const override { return "rules.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void preview(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void keep(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  /** The form as rules; none (said) when a field is wrong. */
  std::optional<fees::Rules> read();
  void say(const std::string &error, const std::string &notice = "");

  Online &online_;
  FeeDesk &desk_;
  std::string name_;
  std::string keepBack_;
  std::string byBlocks_;
  std::string byFees_;
  std::string byUptime_;
  std::string equal_;
  std::string missPenalty_;
  std::string minPayout_;
  bool registeredOnly_{true};
  std::string inForce_;
  std::vector<FeeFact> totals_;
  std::vector<PayoutRow> payouts_;
  std::string error_;
  std::string notice_;
  bool busy_{false};
};

class PayoutsScreen : public OperationForm {
public:
  PayoutsScreen(Online &online, FeeDesk &desk, std::function<void(const std::string &)> made);
  void willShow() override;

protected:
  std::string modelName() const override { return "payouts"; }
  std::string viewName() const override { return "payouts.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void makePayouts(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  /** After the operation file is saved: keep the round, and its round file beside it. */
  void saved(const std::string &path);

  FeeDesk &desk_;
  std::string title_;
  std::vector<FeeFact> totals_;
  std::vector<PayoutRow> payouts_;
  /** The round the operation being made pays. */
  std::optional<fees::Round> pending_;
};

/**
 * Session::compare for the fee app: payouts from the fee account checked against the round kept in
 * the vault that their metadata names; anything else as the other apps compare it.
 */
void compareWithRounds(Online &online, FeeDesk &desk, const pp::operation::Operation &operation,
                       std::function<void(std::vector<std::pair<std::string, std::string>>)> done);

/** Where a round's file goes beside its operation file: "payouts-3.json" → "payouts-3.round.json". */
std::string roundFilePath(const std::string &operationPath);

} // namespace apps::online
