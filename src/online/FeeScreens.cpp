#include "online/FeeScreens.h"

#include "net/FeeOperations.h"
#include "core/AtomicFile.h"
#include "core/Amounts.h"
#include "ledger/client/AccountIds.h"
#include "ledger/lib/common/BinaryPack.hpp"
#include "ledger/lib/common/Utilities.h"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>

namespace apps::online {

namespace op = pp::operation;

namespace {

const std::vector<ui_host::FileFilter> kRoundFiles = {{"Fee round files", "json"}};
const std::vector<ui_host::FileFilter> kCsvFiles = {{"CSV files", "csv"}};

int indexArg(const ui::VariantList &args) { return args.empty() ? -1 : args[0].Get<int>(-1); }

std::string trimmed(const std::string &text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::string blocksText(const fees::Period &p) {
  return formatAmount(p.fromBlock) + " to " + formatAmount(p.toBlock > 0 ? p.toBlock - 1 : 0);
}

/** The fee for each payout of round `number` ending at `toBlock`: the metadata's length is known ahead. */
pp::Roe<uint64_t> feeFor(const net::ChainInfo &chain, uint64_t number, uint64_t toBlock) {
  fees::Round sample;
  sample.number = number;
  sample.report.period.toBlock = toBlock;
  sample.digest = std::string(64, '0');
  return net::payoutFee(chain, fees::roundMeta(sample));
}

std::vector<PayoutRow> payoutRows(const fees::Table &table, const fees::Round *last) {
  std::map<uint64_t, uint64_t> before;
  if (last) {
    for (const auto &p : last->table.payouts) {
      before[p.account] = p.amount;
    }
  }
  std::vector<PayoutRow> rows;
  for (const auto &p : table.payouts) {
    const auto was = before.find(p.account);
    rows.push_back({formatAmount(p.account), p.who, formatAmount(p.amount),
                    was == before.end() ? "—" : formatAmount(was->second)});
  }
  return rows;
}

std::vector<FeeFact> tableFacts(const fees::Report &report, const fees::Table &table) {
  return {{"Fee account balance", formatAmount(report.feeBalance)},
          {"Kept back", formatAmount(table.keptBack)},
          {"Shared out", formatAmount(table.total) + " to " + std::to_string(table.payouts.size()) + " accounts"},
          {"Fees", formatAmount(table.fees) + " (" + formatAmount(table.feeEach) + " each)"},
          {"Stays in the fee account", formatAmount(table.left)}};
}

} // namespace

// ---- FeeDesk ----

std::string FeeDesk::load() {
  auto book = fees::FeeBook::load(session_.vault);
  book_ = book ? std::move(book.value()) : fees::FeeBook{};
  return book ? "" : book.error().message;
}

pp::Roe<void> FeeDesk::save() { return book_.save(session_.vault); }

fees::Rules FeeDesk::rules() const {
  if (book_.rules) {
    return *book_.rules;
  }
  fees::Rules start;
  start.name = "first rules";
  start.keepBack = 1000;
  start.minersByBlocks = 6000;
  start.relaysByUptime = 3000;
  start.everyoneEqual = 1000;
  start.missPenalty = 1;
  return start;
}

uint64_t FeeDesk::nextNumber() const { return book_.last() ? book_.last()->number + 1 : 1; }

uint64_t FeeDesk::nextStart() const { return book_.last() ? book_.last()->report.period.toBlock : 1; }

// ---- Rounds ----

void RoundsScreen::bind(ui::DataModelConstructor &model) {
  if (auto row = model.RegisterStruct<Row>()) {
    row.RegisterMember("number", &Row::number);
    row.RegisterMember("blocks", &Row::blocks);
    row.RegisterMember("paid", &Row::paid);
    row.RegisterMember("digest", &Row::digest);
  }
  model.RegisterArray<std::vector<Row>>();
  model.Bind("balance", &balance_);
  model.Bind("rows", &rows_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.BindEventCallback("rounds_next", &RoundsScreen::startNext, this);
  model.BindEventCallback("rounds_import", &RoundsScreen::importRound, this);
  model.BindEventCallback("rounds_export", &RoundsScreen::exportRound, this);
}

void RoundsScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void RoundsScreen::willShow() {
  rows_.clear();
  const auto &rounds = desk_.book().rounds;
  for (auto it = rounds.rbegin(); it != rounds.rend(); ++it) {
    rows_.push_back({std::to_string(it->number), blocksText(it->report.period),
                     formatAmount(it->table.total) + " to " + std::to_string(it->table.payouts.size()),
                     it->digest.substr(0, 16)});
  }
  balance_ = "…";
  say("");
  auto balance = std::make_shared<std::string>();
  online_.withChain(
      [balance](net::ChainLink &link) -> std::string {
        auto fee = link.account(pp::AccountIds::ID_FEE);
        if (!fee) {
          return fee.error().message;
        }
        const auto &balances = fee.value().wallet.mBalances;
        const auto native = balances.find(pp::AccountIds::ID_GENESIS);
        *balance = formatAmount(native == balances.end() || native->second < 0 ? 0
                                                                               : static_cast<uint64_t>(native->second));
        return "";
      },
      [this, balance](const std::string &error) {
        balance_ = error.empty() ? *balance : "unknown";
        say(error.empty() ? "" : error + ".");
      });
}

void RoundsScreen::startNext(ui::DataModelHandle, ui::Event &, const ui::VariantList &) { session_.navigate("period"); }

void RoundsScreen::importRound(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  app_->chooseFileToOpen(kRoundFiles, [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    auto text = readFile(*path);
    auto round = text ? fees::roundFromJson(text.value()) : pp::Roe<fees::Round>(text.error());
    if (!round) {
      say(round.error().message + ".");
      return;
    }
    desk_.book().put(round.value());
    if (auto saved = desk_.save(); !saved) {
      say(saved.error().message);
      return;
    }
    willShow();
    say("", "Round " + std::to_string(round.value().number) + " kept: its digest matches its contents.");
  });
}

void RoundsScreen::exportRound(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
  const int index = indexArg(args);
  const auto &rounds = desk_.book().rounds;
  if (index < 0 || index >= static_cast<int>(rounds.size())) {
    return;
  }
  const fees::Round round = rounds[rounds.size() - 1 - static_cast<size_t>(index)];
  app_->chooseFileToSave(kRoundFiles, "round-" + std::to_string(round.number) + ".round.json",
                         [this, round](std::optional<std::string> path) {
                           if (!path) {
                             return;
                           }
                           if (auto written = writeFileAtomic(*path, fees::roundToJson(round)); !written) {
                             say(written.error().message);
                             return;
                           }
                           say("", "Saved: hand it to the other holders, and publish it as you agreed.");
                         });
}

// ---- Period ----

void PeriodScreen::bind(ui::DataModelConstructor &model) {
  if (auto fact = model.RegisterStruct<FeeFact>()) {
    fact.RegisterMember("label", &FeeFact::label);
    fact.RegisterMember("value", &FeeFact::value);
  }
  if (auto row = model.RegisterStruct<MinerRow>()) {
    row.RegisterMember("account", &MinerRow::account);
    row.RegisterMember("blocks", &MinerRow::blocks);
    row.RegisterMember("fees", &MinerRow::fees);
    row.RegisterMember("misses", &MinerRow::misses);
    row.RegisterMember("registered", &MinerRow::registered);
  }
  model.RegisterArray<std::vector<FeeFact>>();
  model.RegisterArray<std::vector<MinerRow>>();
  model.Bind("number", &number_);
  model.Bind("from", &from_);
  model.Bind("ends_at", &endsAt_);
  model.Bind("facts", &facts_);
  model.Bind("miners", &miners_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
  model.BindEventCallback("period_read", &PeriodScreen::read, this);
}

void PeriodScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void PeriodScreen::willShow() {
  number_ = std::to_string(desk_.nextNumber());
  if (from_.empty()) {
    from_ = std::to_string(desk_.nextStart());
  }
  facts_.clear();
  miners_.clear();
  if (desk_.book().draft) {
    show(*desk_.book().draft);
    say("", "Read so far; read again to bring it up to date.");
  } else {
    say("", "Choose where the period ends, and read the chain.");
  }
}

void PeriodScreen::show(const fees::Report &report) {
  facts_ = {{"Blocks", blocksText(report.period) + " (" + formatAmount(report.blocks) + " blocks)"},
            {"Slots", formatAmount(report.period.firstSlot) + " to " + formatAmount(report.period.lastSlot)},
            {"Fees in them", formatAmount(report.fees)},
            {"Fee account balance", formatAmount(report.feeBalance)}};
  miners_.clear();
  for (const auto &m : report.miners) {
    miners_.push_back({formatAmount(m.account), formatAmount(m.blocks), formatAmount(m.fees), formatAmount(m.misses),
                       m.registered ? "yes" : "no"});
  }
  refresh();
}

void PeriodScreen::read(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (busy_) {
    return;
  }
  auto from = parseAmount(from_);
  if (!from || from.value() == 0) {
    say("Type the block the period starts at (1 or later).");
    return;
  }
  std::optional<int64_t> endsAt;
  if (!trimmed(endsAt_).empty()) {
    endsAt = parseUtcDate(endsAt_);
    if (!endsAt) {
      say("Type the end as 2026-10-31 or 2026-10-31 18:00 (UTC), or leave it empty for now.");
      return;
    }
  }
  std::string warning;
  if (from.value() != desk_.nextStart()) {
    warning = desk_.book().last() ? " The last round ended before block " + formatAmount(desk_.nextStart()) +
                                        ": this period " +
                                        (from.value() < desk_.nextStart() ? "overlaps it." : "leaves a gap.")
                                  : "";
  }
  busy_ = true;
  say("", "Reading the chain…");
  auto report = std::make_shared<fees::Report>();
  const uint64_t start = from.value();
  online_.withChain(
      [this, report, start, endsAt](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        uint64_t to = chain.value().nextBlockId;
        if (endsAt) {
          auto at = net::blockAt(link, chain.value(), *endsAt);
          if (!at) {
            return at.error().message;
          }
          to = at.value();
        }
        auto read = net::readPeriod(link, start, to, [this](uint64_t done, uint64_t total) {
          app_->post([this, done, total] {
            notice_ = "Reading the chain: " + formatAmount(done) + " of " + formatAmount(total) + " blocks…";
            refresh();
          });
        });
        if (!read) {
          return read.error().message;
        }
        *report = std::move(read.value());
        return "";
      },
      [this, report, warning](const std::string &error) {
        busy_ = false;
        if (!error.empty()) {
          say(error + ".");
          return;
        }
        // The relays from the list, with the measures already entered for them.
        auto &book = desk_.book();
        std::map<std::string, uint32_t> entered;
        if (book.draft) {
          for (const auto &line : book.draft->relays) {
            entered[line.relay.peerId] = line.uptime;
          }
          report->relaySource = book.draft->relaySource;
          report->relaysEnteredBy = book.draft->relaysEnteredBy;
        }
        for (const auto &relay : book.relays) {
          const auto was = entered.find(relay.peerId);
          report->relays.push_back({relay, was == entered.end() ? 0 : was->second});
        }
        book.draft = *report;
        if (auto saved = desk_.save(); !saved) {
          say(saved.error().message);
          return;
        }
        show(*report);
        say("", "Read: next, the relays' measures (Relays), then the rules." + warning);
      });
}

// ---- Relays ----

void RelaysScreen::bind(ui::DataModelConstructor &model) {
  if (auto row = model.RegisterStruct<RelayRow>()) {
    row.RegisterMember("name", &RelayRow::name);
    row.RegisterMember("peer_id", &RelayRow::peerId);
    row.RegisterMember("account", &RelayRow::account);
    row.RegisterMember("uptime", &RelayRow::uptime);
  }
  model.RegisterArray<std::vector<RelayRow>>();
  model.Bind("rows", &rows_);
  model.Bind("new_name", &newName_);
  model.Bind("new_peer_id", &newPeerId_);
  model.Bind("new_account", &newAccount_);
  model.Bind("source", &source_);
  model.Bind("entered_by", &enteredBy_);
  model.Bind("has_draft", &hasDraft_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.BindEventCallback("relays_add", &RelaysScreen::add, this);
  model.BindEventCallback("relays_remove", &RelaysScreen::remove, this);
  model.BindEventCallback("relays_import", &RelaysScreen::importCsv, this);
  model.BindEventCallback("relays_keep", &RelaysScreen::keep, this);
}

void RelaysScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void RelaysScreen::willShow() {
  const auto &book = desk_.book();
  hasDraft_ = book.draft.has_value();
  std::map<std::string, uint32_t> entered;
  if (book.draft) {
    for (const auto &line : book.draft->relays) {
      entered[line.relay.peerId] = line.uptime;
    }
    source_ = book.draft->relaySource;
    enteredBy_ = book.draft->relaysEnteredBy;
  }
  rows_.clear();
  for (const auto &relay : book.relays) {
    const auto was = entered.find(relay.peerId);
    rows_.push_back({relay.name, relay.peerId, std::to_string(relay.account),
                     was == entered.end() || was->second == 0 ? "" : fees::formatPercent(was->second)});
  }
  say("", hasDraft_ ? "" : "Read the period first to enter this round's measures; the list can change any time.");
}

bool RelaysScreen::apply() {
  std::vector<fees::RelayLine> lines;
  for (const auto &row : rows_) {
    auto account = parseAmount(row.account);
    if (trimmed(row.name).empty() || !account || account.value() == 0) {
      say("Each relay needs a name and the account it is paid to.");
      return false;
    }
    uint32_t uptime = 0;
    if (!trimmed(row.uptime).empty()) {
      auto parsed = fees::parsePercent(row.uptime);
      if (!parsed) {
        say(row.name + ": " + parsed.error().message + ".");
        return false;
      }
      uptime = parsed.value();
    }
    lines.push_back({{trimmed(row.name), trimmed(row.peerId), account.value()}, uptime});
  }
  auto &book = desk_.book();
  book.relays.clear();
  for (const auto &line : lines) {
    book.relays.push_back(line.relay);
  }
  if (book.draft) {
    book.draft->relays = lines;
    book.draft->relaySource = trimmed(source_);
    book.draft->relaysEnteredBy = trimmed(enteredBy_);
  }
  if (auto saved = desk_.save(); !saved) {
    say(saved.error().message);
    return false;
  }
  return true;
}

void RelaysScreen::add(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  auto account = parseAmount(newAccount_);
  if (trimmed(newName_).empty() || !account || account.value() == 0) {
    say("Type the relay's name and the account it is paid to (its peer id, from its operator, too).");
    return;
  }
  rows_.push_back({trimmed(newName_), trimmed(newPeerId_), std::to_string(account.value()), ""});
  if (apply()) {
    newName_.clear();
    newPeerId_.clear();
    newAccount_.clear();
    say("", "Added.");
  }
}

void RelaysScreen::remove(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
  const int index = indexArg(args);
  if (index < 0 || index >= static_cast<int>(rows_.size())) {
    return;
  }
  rows_.erase(rows_.begin() + index);
  if (apply()) {
    say("", "Removed.");
  }
}

void RelaysScreen::importCsv(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  app_->chooseFileToOpen(kCsvFiles, [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    auto text = readFile(*path);
    if (!text) {
      say(text.error().message);
      return;
    }
    // Lines "relay (name or peer id),uptime%": the relays not listed keep what they have.
    size_t matched = 0;
    std::vector<std::string> unknown;
    std::istringstream lines(text.value());
    for (std::string line; std::getline(lines, line);) {
      line = trimmed(line.substr(0, line.find('#')));
      const auto comma = line.find(',');
      if (line.empty() || comma == std::string::npos) {
        continue;
      }
      const std::string who = trimmed(line.substr(0, comma));
      const std::string uptime = trimmed(line.substr(comma + 1));
      if (!fees::parsePercent(uptime)) {
        continue; // a header line
      }
      bool found = false;
      for (auto &row : rows_) {
        if (row.name == who || (!row.peerId.empty() && row.peerId == who)) {
          row.uptime = uptime;
          found = true;
        }
      }
      if (found) {
        ++matched;
      } else {
        unknown.push_back(who);
      }
    }
    std::string notice = std::to_string(matched) + " relays' uptime read from " +
                         std::filesystem::path(*path).filename().string() + ".";
    if (!unknown.empty()) {
      notice += " Not in the list: " + unknown.front() + (unknown.size() > 1 ? " and others" : "") + ".";
    }
    if (source_.empty()) {
      source_ = std::filesystem::path(*path).filename().string();
    }
    say("", notice + " Check them, then keep them.");
  });
}

void RelaysScreen::keep(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (hasDraft_ && trimmed(enteredBy_).empty()) {
    say("Say who entered the measures: they are taken on trust, and the round file names you.");
    return;
  }
  if (apply()) {
    say("", hasDraft_ ? "Kept for this round: next, the rules." : "The list is kept.");
  }
}

// ---- Rules ----

void RulesScreen::bind(ui::DataModelConstructor &model) {
  if (auto fact = model.RegisterStruct<FeeFact>()) {
    fact.RegisterMember("label", &FeeFact::label);
    fact.RegisterMember("value", &FeeFact::value);
  }
  if (auto row = model.RegisterStruct<PayoutRow>()) {
    row.RegisterMember("account", &PayoutRow::account);
    row.RegisterMember("who", &PayoutRow::who);
    row.RegisterMember("amount", &PayoutRow::amount);
    row.RegisterMember("last", &PayoutRow::last);
  }
  model.RegisterArray<std::vector<FeeFact>>();
  model.RegisterArray<std::vector<PayoutRow>>();
  model.Bind("name", &name_);
  model.Bind("keep_back", &keepBack_);
  model.Bind("by_blocks", &byBlocks_);
  model.Bind("by_fees", &byFees_);
  model.Bind("by_uptime", &byUptime_);
  model.Bind("equal", &equal_);
  model.Bind("miss_penalty", &missPenalty_);
  model.Bind("min_payout", &minPayout_);
  model.Bind("registered_only", &registeredOnly_);
  model.Bind("in_force", &inForce_);
  model.Bind("totals", &totals_);
  model.Bind("payouts", &payouts_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
  model.BindEventCallback("rules_preview", &RulesScreen::preview, this);
  model.BindEventCallback("rules_keep", &RulesScreen::keep, this);
}

void RulesScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void RulesScreen::willShow() {
  const fees::Rules rules = desk_.rules();
  name_ = rules.name;
  keepBack_ = fees::formatPercent(rules.keepBack);
  byBlocks_ = fees::formatPercent(rules.minersByBlocks);
  byFees_ = fees::formatPercent(rules.minersByFees);
  byUptime_ = fees::formatPercent(rules.relaysByUptime);
  equal_ = fees::formatPercent(rules.everyoneEqual);
  missPenalty_ = formatAmount(rules.missPenalty);
  minPayout_ = formatAmount(rules.minPayout);
  registeredOnly_ = rules.registeredOnly;
  inForce_ = desk_.book().rules ? "\"" + rules.name + "\" (" + fees::rulesHash(rules).substr(0, 16) + ")"
                                : "none kept yet: these are a starting point";
  totals_.clear();
  payouts_.clear();
  say("");
}

std::optional<fees::Rules> RulesScreen::read() {
  fees::Rules rules;
  rules.name = trimmed(name_);
  struct Field {
    const std::string &text;
    uint32_t &into;
    const char *title;
  };
  for (const Field &f : {Field{keepBack_, rules.keepBack, "Kept back"}, Field{byBlocks_, rules.minersByBlocks, "Miners, by blocks"},
                         Field{byFees_, rules.minersByFees, "Miners, by fees"},
                         Field{byUptime_, rules.relaysByUptime, "Relays, by uptime"},
                         Field{equal_, rules.everyoneEqual, "Everyone, equally"}}) {
    auto value = fees::parsePercent(f.text);
    if (!value) {
      say(std::string(f.title) + ": " + value.error().message + ".");
      return std::nullopt;
    }
    f.into = value.value();
  }
  auto penalty = parseAmount(missPenalty_);
  auto minimum = parseAmount(minPayout_);
  if (!penalty || !minimum) {
    say("Type the blocks a miss costs and the smallest payout as numbers.");
    return std::nullopt;
  }
  rules.missPenalty = penalty.value();
  rules.minPayout = minimum.value();
  rules.registeredOnly = registeredOnly_;
  if (rules.name.empty()) {
    say("Name the rules (\"autumn 2026\", say): rounds name the rules they used.");
    return std::nullopt;
  }
  if (auto ok = fees::check(rules); !ok) {
    say(ok.error().message + ".");
    return std::nullopt;
  }
  return rules;
}

void RulesScreen::preview(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  auto rules = read();
  if (!rules || busy_) {
    return;
  }
  const auto &draft = desk_.book().draft;
  if (!draft) {
    say("Read the period first: the preview pays this round's report.");
    return;
  }
  busy_ = true;
  say("", "Working the payouts out…");
  auto feeEach = std::make_shared<uint64_t>(0);
  const uint64_t number = desk_.nextNumber();
  const uint64_t toBlock = draft->period.toBlock;
  online_.withChain(
      [feeEach, number, toBlock](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        auto fee = feeFor(chain.value(), number, toBlock);
        if (!fee) {
          return fee.error().message;
        }
        *feeEach = fee.value();
        return "";
      },
      [this, rules = *rules, feeEach](const std::string &error) {
        busy_ = false;
        if (!error.empty()) {
          say(error + ".");
          return;
        }
        const auto &report = *desk_.book().draft;
        auto table = fees::computePayouts(report, rules, *feeEach);
        if (!table) {
          say(table.error().message + ".");
          return;
        }
        totals_ = tableFacts(report, table.value());
        payouts_ = payoutRows(table.value(), desk_.book().last());
        say("", "The payouts these rules make (the last round's beside them).");
      });
}

void RulesScreen::keep(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  auto rules = read();
  if (!rules) {
    return;
  }
  desk_.book().rules = *rules;
  if (auto saved = desk_.save(); !saved) {
    say(saved.error().message);
    return;
  }
  inForce_ = "\"" + rules->name + "\" (" + fees::rulesHash(*rules).substr(0, 16) + ")";
  say("", "Kept as the rules in force: the next rounds use them until they change.");
}

// ---- Payouts ----

PayoutsScreen::PayoutsScreen(Online &online, FeeDesk &desk, std::function<void(const std::string &)> made)
    : OperationForm(online,
                    [this, made = std::move(made)](const std::string &path) {
                      saved(path);
                      made(path);
                    }),
      desk_(desk) {}

void PayoutsScreen::bind(ui::DataModelConstructor &model) {
  bindForm(model);
  if (auto fact = model.RegisterStruct<FeeFact>()) {
    fact.RegisterMember("label", &FeeFact::label);
    fact.RegisterMember("value", &FeeFact::value);
  }
  if (auto row = model.RegisterStruct<PayoutRow>()) {
    row.RegisterMember("account", &PayoutRow::account);
    row.RegisterMember("who", &PayoutRow::who);
    row.RegisterMember("amount", &PayoutRow::amount);
    row.RegisterMember("last", &PayoutRow::last);
  }
  model.RegisterArray<std::vector<FeeFact>>();
  model.RegisterArray<std::vector<PayoutRow>>();
  model.Bind("title", &title_);
  model.Bind("totals", &totals_);
  model.Bind("payouts", &payouts_);
  model.BindEventCallback("payouts_make", &PayoutsScreen::makePayouts, this);
}

void PayoutsScreen::willShow() {
  totals_.clear();
  payouts_.clear();
  pending_.reset();
  const auto &book = desk_.book();
  if (!book.draft) {
    title_ = "No round under way";
    say("Read the next round's period first (Period).");
    return;
  }
  if (!book.rules) {
    title_ = "No rules kept";
    say("Keep the rules first (Rules): the round names the rules it pays by.");
    return;
  }
  fees::Round round;
  round.number = desk_.nextNumber();
  round.report = *book.draft;
  round.rules = *book.rules;
  title_ = "Round " + std::to_string(round.number) + ": blocks " + blocksText(round.report.period);
  say("", "Working the payouts out…");
  auto shared = std::make_shared<fees::Round>(round);
  online_.withChain(
      [shared](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        auto fee = feeFor(chain.value(), shared->number, shared->report.period.toBlock);
        if (!fee) {
          return fee.error().message;
        }
        auto table = fees::computePayouts(shared->report, shared->rules, fee.value());
        if (!table) {
          return table.error().message;
        }
        shared->table = std::move(table.value());
        shared->digest = fees::roundDigest(shared->report, shared->rules, shared->table);
        return "";
      },
      [this, shared](const std::string &error) {
        if (!error.empty()) {
          say(error + ".");
          return;
        }
        totals_ = tableFacts(shared->report, shared->table);
        totals_.push_back({"Rules", "\"" + shared->rules.name + "\" (" + fees::rulesHash(shared->rules).substr(0, 16) + ")"});
        totals_.push_back({"Relays' measures",
                           shared->report.relays.empty()
                               ? "none"
                               : "entered by " + shared->report.relaysEnteredBy + ", from " + shared->report.relaySource});
        totals_.push_back({"Round digest", shared->digest.substr(0, 16) + "…"});
        payouts_ = payoutRows(shared->table, desk_.book().last());
        pending_ = *shared;
        say("", "Each payout will carry the round's digest. Make the operation for the fee account's holders.");
      });
}

void PayoutsScreen::makePayouts(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (!pending_) {
    say("Nothing to pay yet: the payouts are being worked out, or cannot be.");
    return;
  }
  pending_->madeAt = nowSeconds();
  const fees::Round round = *pending_;
  make(
      [round](net::ChainLink &link, const net::ChainInfo &chain, int64_t validFor,
              const std::string &notes) -> pp::Roe<op::Operation> {
        auto signers = link.signersOf(pp::AccountIds::ID_FEE);
        if (!signers) {
          return signers.error();
        }
        auto fee = link.account(pp::AccountIds::ID_FEE);
        if (!fee) {
          return fee.error();
        }
        const auto &balances = fee.value().wallet.mBalances;
        const auto native = balances.find(pp::AccountIds::ID_GENESIS);
        const uint64_t balance =
            native == balances.end() || native->second < 0 ? 0 : static_cast<uint64_t>(native->second);
        return net::makePayouts(chain, signers.value(), balance, round, {nowSeconds(), validFor, notes});
      },
      "payouts-" + std::to_string(round.number) + ".json");
}

void PayoutsScreen::saved(const std::string &path) {
  if (!pending_) {
    return;
  }
  fees::Round round = *pending_;
  round.operationFile = std::filesystem::path(path).filename().string();
  auto &book = desk_.book();
  book.put(round);
  book.draft.reset();
  if (auto kept = desk_.save(); !kept) {
    say(kept.error().message);
    return;
  }
  const std::string roundPath = roundFilePath(path);
  if (auto written = writeFileAtomic(roundPath, fees::roundToJson(round)); !written) {
    say("The round is kept, but its file could not be written: " + written.error().message);
    return;
  }
  pending_.reset();
}

// ---- Review ----

void compareWithRounds(Online &online, FeeDesk &desk, const op::Operation &operation,
                       std::function<void(std::vector<std::pair<std::string, std::string>>)> done) {
  if (operation.kind != op::Kind::Batch && operation.kind != op::Kind::Transfer) {
    online.compare(operation, std::move(done));
    return;
  }
  std::vector<pp::Ledger::TxDefault> transfers;
  for (const auto *record : [&] {
         std::vector<const pp::Ledger::Record *> all{&operation.record};
         for (const auto &more : operation.moreRecords) {
           all.push_back(&more);
         }
         return all;
       }()) {
    auto tx = pp::utl::binaryUnpack<pp::Ledger::TxDefault>(record->data);
    if (!tx) {
      done({{"Cannot compare", "a transfer does not decode"}});
      return;
    }
    transfers.push_back(tx.value());
  }
  const auto mark = fees::parseRoundMeta(transfers.front().meta);
  if (transfers.front().fromWalletId != pp::AccountIds::ID_FEE || !mark) {
    online.compare(operation, std::move(done));
    return;
  }
  const fees::Round *round = nullptr;
  for (const auto &r : desk.book().rounds) {
    if (r.digest == mark->digest) {
      round = &r;
    }
  }
  std::vector<std::pair<std::string, std::string>> lines;
  lines.emplace_back("Round", std::to_string(mark->number) + ": blocks up to " +
                                 formatAmount(mark->toBlock > 0 ? mark->toBlock - 1 : 0));
  if (!round) {
    lines.emplace_back("Its round file", "not kept here: import it (Rounds) to check these payouts against it");
    done(lines);
    return;
  }
  // Every transfer carries the round's mark, and the transfers are the round's payouts, exactly.
  std::map<uint64_t, uint64_t> expected;
  for (const auto &p : round->table.payouts) {
    expected[p.account] = p.amount;
  }
  std::map<uint64_t, uint64_t> paid;
  bool marked = true;
  for (const auto &tx : transfers) {
    paid[tx.toWalletId] += tx.amount;
    marked = marked && tx.meta == transfers.front().meta && tx.tokenId == 0;
  }
  lines.emplace_back("Period", "blocks " + blocksText(round->report.period));
  lines.emplace_back("Rules", "\"" + round->rules.name + "\" (" + fees::rulesHash(round->rules).substr(0, 16) + ")");
  lines.emplace_back("Against its round file", paid == expected && marked
                                                   ? "the same payouts, account by account"
                                                   : "DIFFERENT payouts: do not sign; ask who made it");
  done(lines);
}

std::string roundFilePath(const std::string &operationPath) {
  std::filesystem::path path(operationPath);
  return (path.parent_path() / (path.stem().string() + ".round.json")).string();
}

} // namespace apps::online
