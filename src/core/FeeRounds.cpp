#include "core/FeeRounds.h"

#include "core/MulDiv.h"

#include "lib/common/Utilities.h"
#include "common/io/Json.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <memory>

namespace apps::fees {

using pp::common::Object;
using pp::common::Value;

namespace {

constexpr uint32_t kRoundVersion = 1;
constexpr const char *kMetaPrefix = "pp-fee-round/1";

std::string trimmed(const std::string &text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}


// ---- JSON: written in a fixed order, so the digest is the same everywhere ----

std::shared_ptr<Object> periodObject(const Period &p) {
  auto o = std::make_shared<Object>();
  o->setJsonUInt("fromBlock", p.fromBlock);
  o->setJsonUInt("toBlock", p.toBlock);
  o->setJsonUInt("firstSlot", p.firstSlot);
  o->setJsonUInt("lastSlot", p.lastSlot);
  return o;
}

std::shared_ptr<Object> relayObject(const Relay &r) {
  auto o = std::make_shared<Object>();
  o->set("name", r.name);
  o->set("peerId", r.peerId);
  o->setJsonUInt("account", r.account);
  return o;
}

std::shared_ptr<Object> reportObject(const Report &r) {
  auto o = std::make_shared<Object>();
  o->set("period", *periodObject(r.period));
  o->setJsonUInt("blocks", r.blocks);
  o->setJsonUInt("fees", r.fees);
  o->setJsonUInt("feeBalance", r.feeBalance);
  std::vector<Value> miners;
  for (const auto &m : r.miners) {
    auto e = std::make_shared<Object>();
    e->setJsonUInt("account", m.account);
    e->setJsonUInt("blocks", m.blocks);
    e->setJsonUInt("fees", m.fees);
    e->setJsonUInt("misses", m.misses);
    e->set("registered", m.registered);
    miners.emplace_back(std::move(e));
  }
  o->set("miners", Object::array(std::move(miners)));
  std::vector<Value> relays;
  for (const auto &line : r.relays) {
    auto e = relayObject(line.relay);
    e->setJsonUInt("uptime", line.uptime);
    relays.emplace_back(std::move(e));
  }
  o->set("relays", Object::array(std::move(relays)));
  o->set("relaySource", r.relaySource);
  o->set("relaysEnteredBy", r.relaysEnteredBy);
  return o;
}

std::shared_ptr<Object> rulesObject(const Rules &r) {
  auto o = std::make_shared<Object>();
  o->set("name", r.name);
  o->setJsonUInt("keepBack", r.keepBack);
  o->setJsonUInt("minersByBlocks", r.minersByBlocks);
  o->setJsonUInt("minersByFees", r.minersByFees);
  o->setJsonUInt("relaysByUptime", r.relaysByUptime);
  o->setJsonUInt("everyoneEqual", r.everyoneEqual);
  o->setJsonUInt("missPenalty", r.missPenalty);
  o->setJsonUInt("minPayout", r.minPayout);
  o->set("registeredOnly", r.registeredOnly);
  return o;
}

std::shared_ptr<Object> tableObject(const Table &t) {
  auto o = std::make_shared<Object>();
  o->setJsonUInt("keptBack", t.keptBack);
  o->setJsonUInt("pot", t.pot);
  o->setJsonUInt("feeEach", t.feeEach);
  std::vector<Value> payouts;
  for (const auto &p : t.payouts) {
    auto e = std::make_shared<Object>();
    e->setJsonUInt("account", p.account);
    e->set("who", p.who);
    e->setJsonUInt("amount", p.amount);
    std::vector<Value> shares;
    for (uint64_t s : p.byShare) {
      shares.emplace_back(static_cast<int64_t>(s));
    }
    e->set("byShare", Object::array(std::move(shares)));
    payouts.emplace_back(std::move(e));
  }
  o->set("payouts", Object::array(std::move(payouts)));
  o->setJsonUInt("total", t.total);
  o->setJsonUInt("fees", t.fees);
  o->setJsonUInt("left", t.left);
  return o;
}

std::shared_ptr<Object> roundObject(const Round &r) {
  auto o = std::make_shared<Object>();
  o->set("format", kRoundFormat);
  o->setJsonUInt("version", kRoundVersion);
  o->setJsonUInt("number", r.number);
  o->set("madeAt", static_cast<int64_t>(r.madeAt));
  o->set("operationFile", r.operationFile);
  o->set("digest", r.digest);
  o->set("rulesHash", rulesHash(r.rules));
  o->set("report", *reportObject(r.report));
  o->set("rules", *rulesObject(r.rules));
  o->set("table", *tableObject(r.table));
  return o;
}

// ---- reading ----

uint64_t u64(const Object &o, const char *key, bool &ok) {
  auto v = o.getNonNegInt(key);
  ok = ok && v.has_value();
  return v.value_or(0);
}

bool flag(const Object &o, const char *key, bool &ok) {
  if (!o.contains(key)) {
    ok = false;
    return false;
  }
  return o.getOrDefault(key, false);
}

std::optional<Relay> readRelay(const Object &o) {
  bool ok = true;
  Relay r;
  r.name = o.getString("name").value_or("");
  r.peerId = o.getString("peerId").value_or("");
  r.account = u64(o, "account", ok);
  return ok && !r.name.empty() ? std::optional<Relay>(r) : std::nullopt;
}

std::optional<Report> readReport(const Object *o) {
  if (!o) {
    return std::nullopt;
  }
  bool ok = true;
  Report r;
  if (const Object *p = o->getObject("period")) {
    r.period.fromBlock = u64(*p, "fromBlock", ok);
    r.period.toBlock = u64(*p, "toBlock", ok);
    r.period.firstSlot = u64(*p, "firstSlot", ok);
    r.period.lastSlot = u64(*p, "lastSlot", ok);
  } else {
    ok = false;
  }
  r.blocks = u64(*o, "blocks", ok);
  r.fees = u64(*o, "fees", ok);
  r.feeBalance = u64(*o, "feeBalance", ok);
  if (const auto *miners = o->getArray("miners")) {
    for (const auto &value : miners->elements) {
      const Object *e = pp::common::asObject(value);
      if (!e) {
        return std::nullopt;
      }
      MinerLine m;
      m.account = u64(*e, "account", ok);
      m.blocks = u64(*e, "blocks", ok);
      m.fees = u64(*e, "fees", ok);
      m.misses = u64(*e, "misses", ok);
      m.registered = flag(*e, "registered", ok);
      r.miners.push_back(m);
    }
  }
  if (const auto *relays = o->getArray("relays")) {
    for (const auto &value : relays->elements) {
      const Object *e = pp::common::asObject(value);
      auto relay = e ? readRelay(*e) : std::nullopt;
      if (!relay) {
        return std::nullopt;
      }
      const uint64_t uptime = u64(*e, "uptime", ok);
      if (uptime > kWhole) {
        return std::nullopt;
      }
      r.relays.push_back({*relay, static_cast<uint32_t>(uptime)});
    }
  }
  r.relaySource = o->getString("relaySource").value_or("");
  r.relaysEnteredBy = o->getString("relaysEnteredBy").value_or("");
  return ok ? std::optional<Report>(r) : std::nullopt;
}

std::optional<Rules> readRules(const Object *o) {
  if (!o) {
    return std::nullopt;
  }
  bool ok = true;
  Rules r;
  r.name = o->getString("name").value_or("");
  auto bp = [&](const char *key) {
    const uint64_t v = u64(*o, key, ok);
    ok = ok && v <= kWhole;
    return static_cast<uint32_t>(v);
  };
  r.keepBack = bp("keepBack");
  r.minersByBlocks = bp("minersByBlocks");
  r.minersByFees = bp("minersByFees");
  r.relaysByUptime = bp("relaysByUptime");
  r.everyoneEqual = bp("everyoneEqual");
  r.missPenalty = u64(*o, "missPenalty", ok);
  r.minPayout = u64(*o, "minPayout", ok);
  r.registeredOnly = flag(*o, "registeredOnly", ok);
  return ok ? std::optional<Rules>(r) : std::nullopt;
}

std::optional<Table> readTable(const Object *o) {
  if (!o) {
    return std::nullopt;
  }
  bool ok = true;
  Table t;
  t.keptBack = u64(*o, "keptBack", ok);
  t.pot = u64(*o, "pot", ok);
  t.feeEach = u64(*o, "feeEach", ok);
  if (const auto *payouts = o->getArray("payouts")) {
    for (const auto &value : payouts->elements) {
      const Object *e = pp::common::asObject(value);
      if (!e) {
        return std::nullopt;
      }
      Payout p;
      p.account = u64(*e, "account", ok);
      p.who = e->getString("who").value_or("");
      p.amount = u64(*e, "amount", ok);
      if (const auto *shares = e->getArray("byShare")) {
        for (const auto &s : shares->elements) {
          const auto n = pp::common::asNonNegInt(s);
          ok = ok && n.has_value();
          p.byShare.push_back(n.value_or(0));
        }
      }
      t.payouts.push_back(std::move(p));
    }
  }
  t.total = u64(*o, "total", ok);
  t.fees = u64(*o, "fees", ok);
  t.left = u64(*o, "left", ok);
  return ok ? std::optional<Table>(t) : std::nullopt;
}

pp::Roe<Round> readRound(const Object &o) {
  if (o.getString("format").value_or("") != kRoundFormat) {
    return pp::Error("Not a fee round file");
  }
  if (o.getNonNegInt("version").value_or(0) != kRoundVersion) {
    return pp::Error("A fee round file of another version");
  }
  Round r;
  bool ok = true;
  r.number = u64(o, "number", ok);
  r.madeAt = o.getOrDefault("madeAt", int64_t{0});
  r.operationFile = o.getString("operationFile").value_or("");
  r.digest = o.getString("digest").value_or("");
  auto report = readReport(o.getObject("report"));
  auto rules = readRules(o.getObject("rules"));
  auto table = readTable(o.getObject("table"));
  if (!ok || !report || !rules || !table) {
    return pp::Error("The fee round file is damaged");
  }
  r.report = std::move(*report);
  r.rules = std::move(*rules);
  r.table = std::move(*table);
  if (roundDigest(r.report, r.rules, r.table) != r.digest) {
    return pp::Error("The fee round file does not match its digest: it was changed");
  }
  return r;
}

} // namespace

pp::Roe<uint32_t> parsePercent(const std::string &text) {
  std::string t = trimmed(text);
  if (!t.empty() && t.back() == '%') {
    t = trimmed(t.substr(0, t.size() - 1));
  }
  const auto dot = t.find('.');
  const std::string whole = t.substr(0, dot);
  const std::string fraction = dot == std::string::npos ? "" : t.substr(dot + 1);
  auto digits = [](const std::string &s) {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
  };
  if (whole.empty() || whole.size() > 3 || fraction.size() > 2 || !digits(whole) || !digits(fraction) ||
      (dot != std::string::npos && fraction.empty())) {
    return pp::Error("Type a percentage: 0 to 100, two decimals at most");
  }
  uint32_t bp = static_cast<uint32_t>(std::stoul(whole)) * 100;
  if (!fraction.empty()) {
    bp += static_cast<uint32_t>(std::stoul(fraction)) * (fraction.size() == 1 ? 10 : 1);
  }
  if (bp > kWhole) {
    return pp::Error("A percentage is at most 100");
  }
  return bp;
}

std::string formatPercent(uint32_t basisPoints) {
  std::string text = std::to_string(basisPoints / 100);
  const uint32_t fraction = basisPoints % 100;
  if (fraction != 0) {
    text += "." + std::string(fraction < 10 ? "0" : "") + std::to_string(fraction);
    if (text.back() == '0') {
      text.pop_back();
    }
  }
  return text + "%";
}

pp::Roe<void> check(const Rules &rules) {
  if (rules.keepBack > kWhole) {
    return pp::Error("Keep back at most 100%");
  }
  const uint64_t shares = uint64_t{rules.minersByBlocks} + rules.minersByFees + rules.relaysByUptime +
                          rules.everyoneEqual;
  if (shares != kWhole) {
    return pp::Error("The shares add up to " + formatPercent(static_cast<uint32_t>(std::min<uint64_t>(shares, 99999))) +
                     ", not 100%");
  }
  return {};
}

std::string rulesHash(const Rules &rules) {
  return pp::utl::hexEncode(pp::utl::sha256Raw(pp::common::io::objectToJsonString(*rulesObject(rules))));
}

std::vector<std::string> shareTitles() {
  return {"Miners, by blocks", "Miners, by fees", "Relays, by uptime", "Everyone, equally"};
}

pp::Roe<Table> computePayouts(const Report &report, const Rules &rules, uint64_t feeEach) {
  if (auto ok = check(rules); !ok) {
    return ok.error();
  }
  // Who is paid this round, and their weight in each share (shareTitles order).
  struct Recipient {
    std::vector<std::string> names;
    bool miner{false};
    std::array<uint64_t, kShares> weight{};
  };
  std::map<uint64_t, Recipient> recipients;
  for (const auto &m : report.miners) {
    if (m.blocks == 0 || (rules.registeredOnly && !m.registered)) {
      continue;
    }
    auto &r = recipients[m.account];
    r.miner = true;
    r.weight[0] += productReaches(m.misses, rules.missPenalty, m.blocks) ? 0 : m.blocks - m.misses * rules.missPenalty;
    r.weight[1] += m.fees;
    r.weight[3] = 1;
  }
  for (const auto &line : report.relays) {
    if (line.uptime == 0 || line.relay.account == 0) {
      continue;
    }
    auto &r = recipients[line.relay.account];
    r.names.push_back(line.relay.name);
    r.weight[2] += line.uptime;
    r.weight[3] = 1;
  }

  Table table;
  table.feeEach = feeEach;
  table.keptBack = mulDiv(report.feeBalance, rules.keepBack, kWhole);
  const uint64_t maxFees = feeEach * recipients.size();
  if (feeEach != 0 && maxFees / feeEach != recipients.size()) {
    return pp::Error("The fees overflow");
  }
  if (report.feeBalance < table.keptBack + maxFees) {
    return pp::Error("The fee account holds " + std::to_string(report.feeBalance) +
                     ": not enough for what is kept back and the payouts' fees");
  }
  table.pot = report.feeBalance - table.keptBack - maxFees;

  const std::array<uint32_t, kShares> shares = {rules.minersByBlocks, rules.minersByFees, rules.relaysByUptime,
                                                rules.everyoneEqual};
  std::array<uint64_t, kShares> totals{};
  for (const auto &[account, r] : recipients) {
    for (size_t k = 0; k < kShares; ++k) {
      totals[k] += r.weight[k];
    }
  }
  const uint64_t minimum = std::max<uint64_t>(rules.minPayout, 1);
  for (const auto &[account, r] : recipients) {
    Payout p;
    p.account = account;
    std::string who = r.miner ? "miner" : "";
    for (const auto &name : r.names) {
      who += (who.empty() ? "" : ", ") + name;
    }
    p.who = who;
    p.byShare.assign(kShares, 0);
    for (size_t k = 0; k < kShares; ++k) {
      if (totals[k] == 0 || r.weight[k] == 0) {
        continue;
      }
      const uint64_t shareAmount = mulDiv(table.pot, shares[k], kWhole);
      p.byShare[k] = mulDiv(shareAmount, r.weight[k], totals[k]);
      p.amount += p.byShare[k];
    }
    if (p.amount >= minimum) {
      table.total += p.amount;
      table.payouts.push_back(std::move(p));
    }
  }
  table.fees = feeEach * table.payouts.size();
  table.left = report.feeBalance - table.total - table.fees;
  return table;
}

std::string roundDigest(const Report &report, const Rules &rules, const Table &table) {
  Object root;
  root.set("report", *reportObject(report));
  root.set("rules", *rulesObject(rules));
  root.set("table", *tableObject(table));
  return pp::utl::hexEncode(pp::utl::sha256Raw(pp::common::io::objectToJsonString(root)));
}

std::string roundMeta(const Round &round) {
  return std::string(kMetaPrefix) + " " + std::to_string(round.number) + " " +
         std::to_string(round.report.period.toBlock) + " " + round.digest;
}

std::optional<RoundMark> parseRoundMeta(const std::string &meta) {
  const std::string prefix = std::string(kMetaPrefix) + " ";
  if (meta.rfind(prefix, 0) != 0) {
    return std::nullopt;
  }
  RoundMark mark;
  std::string rest = meta.substr(prefix.size());
  const auto a = rest.find(' ');
  const auto b = a == std::string::npos ? std::string::npos : rest.find(' ', a + 1);
  if (b == std::string::npos) {
    return std::nullopt;
  }
  const std::string number = rest.substr(0, a);
  const std::string toBlock = rest.substr(a + 1, b - a - 1);
  mark.digest = rest.substr(b + 1);
  auto digits = [](const std::string &s) {
    return !s.empty() && s.size() < 20 &&
           std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
  };
  if (!digits(number) || !digits(toBlock) || mark.digest.size() != 64) {
    return std::nullopt;
  }
  mark.number = std::stoull(number);
  mark.toBlock = std::stoull(toBlock);
  return mark;
}

std::string roundToJson(const Round &round) { return pp::common::io::objectToJsonString(*roundObject(round), 2) + "\n"; }

pp::Roe<Round> roundFromJson(const std::string &json) {
  Object root;
  if (!pp::common::io::objectFromJsonString(root, json)) {
    return pp::Error("Not a fee round file: not JSON");
  }
  return readRound(root);
}

pp::Roe<FeeBook> FeeBook::load(const Vault &vault) {
  auto json = vault.readFile(kFileName);
  if (!json) {
    return json.error();
  }
  FeeBook book;
  if (json.value().empty()) {
    return book;
  }
  Object root;
  if (!pp::common::io::objectFromJsonString(root, json.value())) {
    return pp::Error("The fee app's records are damaged");
  }
  if (const auto *relays = root.getArray("relays")) {
    for (const auto &value : relays->elements) {
      const Object *e = pp::common::asObject(value);
      auto relay = e ? readRelay(*e) : std::nullopt;
      if (!relay) {
        return pp::Error("The fee app's relay list is damaged");
      }
      book.relays.push_back(*relay);
    }
  }
  if (root.getObject("rules")) {
    book.rules = readRules(root.getObject("rules"));
    if (!book.rules) {
      return pp::Error("The fee app's rules are damaged");
    }
  }
  if (root.getObject("draft")) {
    book.draft = readReport(root.getObject("draft"));
    if (!book.draft) {
      return pp::Error("The fee app's draft round is damaged");
    }
  }
  if (const auto *rounds = root.getArray("rounds")) {
    for (const auto &value : rounds->elements) {
      const Object *e = pp::common::asObject(value);
      if (!e) {
        return pp::Error("The fee app's rounds are damaged");
      }
      auto round = readRound(*e);
      if (!round) {
        return pp::Error("A round kept in the vault: " + round.error().message);
      }
      book.rounds.push_back(std::move(round.value()));
    }
  }
  return book;
}

pp::Roe<void> FeeBook::save(Vault &vault) const {
  Object root;
  std::vector<Value> relayList;
  for (const auto &r : relays) {
    relayList.emplace_back(relayObject(r));
  }
  root.set("relays", Object::array(std::move(relayList)));
  if (rules) {
    root.set("rules", *rulesObject(*rules));
  }
  if (draft) {
    root.set("draft", *reportObject(*draft));
  }
  std::vector<Value> roundList;
  for (const auto &r : rounds) {
    roundList.emplace_back(roundObject(r));
  }
  root.set("rounds", Object::array(std::move(roundList)));
  return vault.writeFile(kFileName, pp::common::io::objectToJsonString(root));
}

void FeeBook::put(const Round &round) {
  auto it = std::find_if(rounds.begin(), rounds.end(), [&](const Round &r) { return r.number == round.number; });
  if (it != rounds.end()) {
    *it = round;
  } else {
    rounds.push_back(round);
  }
  std::sort(rounds.begin(), rounds.end(), [](const Round &a, const Round &b) { return a.number < b.number; });
}

const Round *FeeBook::last() const { return rounds.empty() ? nullptr : &rounds.back(); }

} // namespace apps::fees
