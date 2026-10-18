// The fee app's rounds: payouts from a report and rules, round files and their digest, the vault's
// records.
#include "core/FeeRounds.h"
#include "TempDir.h"

#include <gtest/gtest.h>

using namespace apps;
using namespace apps::fees;

namespace {

constexpr uint64_t kA = (1ULL << 20) + 1;
constexpr uint64_t kB = (1ULL << 20) + 2;
constexpr uint64_t kC = (1ULL << 20) + 3;
constexpr uint64_t kRelayAccount = (1ULL << 30) + 900;

/** Three miners (C no longer registered), two relays — one paid to miner A's account. */
Report report() {
  Report r;
  r.period = {101, 2101, 500, 9000};
  r.blocks = 45;
  r.fees = 1000;
  r.feeBalance = 100000;
  r.miners = {{kA, 30, 600, 0, true}, {kB, 10, 400, 2, true}, {kC, 5, 0, 0, false}};
  r.relays = {{{"relay-east", "QmEast", kRelayAccount}, 9900}, {{"relay-west", "QmWest", kA}, 9000}};
  r.relaySource = "monitoring, October";
  r.relaysEnteredBy = "Alice";
  return r;
}

Rules rules() {
  Rules r;
  r.name = "autumn";
  r.keepBack = 1000;
  r.minersByBlocks = 6000;
  r.relaysByUptime = 3000;
  r.everyoneEqual = 1000;
  r.missPenalty = 2;
  return r;
}

const Payout *payoutOf(const Table &t, uint64_t account) {
  for (const auto &p : t.payouts) {
    if (p.account == account) {
      return &p;
    }
  }
  return nullptr;
}

} // namespace

TEST(FeeRoundsTest, PercentagesReadAndPrintInBasisPoints) {
  EXPECT_EQ(parsePercent("12.5").value(), 1250u);
  EXPECT_EQ(parsePercent(" 60 % ").value(), 6000u);
  EXPECT_EQ(parsePercent("0.05").value(), 5u);
  EXPECT_EQ(parsePercent("100").value(), 10000u);
  EXPECT_FALSE(parsePercent("100.01"));
  EXPECT_FALSE(parsePercent("1.234"));
  EXPECT_FALSE(parsePercent("-1"));
  EXPECT_FALSE(parsePercent("12."));
  EXPECT_FALSE(parsePercent(""));
  EXPECT_EQ(formatPercent(1250), "12.5%");
  EXPECT_EQ(formatPercent(6000), "60%");
  EXPECT_EQ(formatPercent(5), "0.05%");
}

TEST(FeeRoundsTest, TheSharesSplitThePotByTheirMeasures) {
  auto table = computePayouts(report(), rules(), 1);
  ASSERT_TRUE(table) << table.error().message;
  const Table &t = table.value();
  // 10% kept back; a fee for each of the three recipients (A, B, the east relay) set aside.
  EXPECT_EQ(t.keptBack, 10000u);
  EXPECT_EQ(t.pot, 89997u);
  ASSERT_EQ(t.payouts.size(), 3u) << "C is no longer registered";
  const Payout *a = payoutOf(t, kA);
  const Payout *b = payoutOf(t, kB);
  const Payout *relay = payoutOf(t, kRelayAccount);
  ASSERT_TRUE(a && b && relay);
  // Blocks (60%): A 30, B 10 less 2 misses × 2 = 6. Relays (30%): by uptime. Everyone (10%): a third each.
  EXPECT_EQ(a->byShare, (std::vector<uint64_t>{44998, 0, 12856, 2999}));
  EXPECT_EQ(a->amount, 60853u);
  EXPECT_EQ(a->who, "miner, relay-west");
  EXPECT_EQ(b->amount, 11998u);
  EXPECT_EQ(relay->amount, 17141u);
  EXPECT_EQ(relay->who, "relay-east");
  EXPECT_EQ(t.total, 89992u);
  EXPECT_EQ(t.fees, 3u);
  EXPECT_EQ(t.left, 10005u) << "what is kept back, and what rounding leaves";
}

TEST(FeeRoundsTest, SmallPayoutsWaitAndAThinAccountRefuses) {
  Rules r = rules();
  r.minPayout = 12000; // B's 11,998 waits for a later round
  auto table = computePayouts(report(), r, 1);
  ASSERT_TRUE(table) << table.error().message;
  EXPECT_EQ(table.value().payouts.size(), 2u);
  EXPECT_EQ(payoutOf(table.value(), kB), nullptr);
  EXPECT_EQ(table.value().fees, 2u);
  EXPECT_EQ(table.value().left, 100000u - table.value().total - 2);

  Report thin = report();
  thin.feeBalance = 2; // not even the three payouts' fees
  EXPECT_FALSE(computePayouts(thin, rules(), 1));

  Rules unbalanced = rules();
  unbalanced.everyoneEqual = 0; // the shares add up to 90%
  EXPECT_FALSE(check(unbalanced));
  EXPECT_FALSE(computePayouts(report(), unbalanced, 1));
}

TEST(FeeRoundsTest, ARoundFileIsCheckedAgainstItsDigest) {
  Round round;
  round.number = 3;
  round.report = report();
  round.rules = rules();
  round.table = computePayouts(round.report, round.rules, 1).value();
  round.digest = roundDigest(round.report, round.rules, round.table);
  round.madeAt = 1'800'000'000;
  round.operationFile = "round-3.json";
  auto back = roundFromJson(roundToJson(round));
  ASSERT_TRUE(back) << back.error().message;
  EXPECT_EQ(back.value().digest, round.digest);
  EXPECT_EQ(back.value().rules, round.rules);
  EXPECT_EQ(back.value().table.total, round.table.total);
  EXPECT_EQ(back.value().report.relays.size(), 2u);
  EXPECT_EQ(back.value().report.relaySource, "monitoring, October");

  // A payout changed by hand no longer matches the digest the chain carries.
  std::string json = roundToJson(round);
  const auto at = json.find("\"amount\": 60853");
  ASSERT_NE(at, std::string::npos);
  json.replace(at, 15, "\"amount\": 70853");
  EXPECT_FALSE(roundFromJson(json));

  // What each transfer carries, and reading it back.
  const std::string meta = roundMeta(round);
  EXPECT_EQ(meta, "pp-fee-round/1 3 2101 " + round.digest);
  auto mark = parseRoundMeta(meta);
  ASSERT_TRUE(mark);
  EXPECT_EQ(mark->number, 3u);
  EXPECT_EQ(mark->toBlock, 2101u);
  EXPECT_EQ(mark->digest, round.digest);
  EXPECT_FALSE(parseRoundMeta("pp-fee-round/1 3 2101 short"));
  EXPECT_FALSE(parseRoundMeta("a note"));

  // The rules' identity changes with any setting.
  Rules other = rules();
  other.minPayout = 1;
  EXPECT_NE(rulesHash(other), rulesHash(rules()));
}

TEST(FeeRoundsTest, TheVaultKeepsRelaysRulesDraftAndRounds) {
  test::TempDir dir;
  Vault vault(dir / "vault", KdfLimits::minimum());
  ASSERT_TRUE(vault.create("a long enough passphrase"));
  ASSERT_TRUE(vault.unlock("a long enough passphrase"));
  auto empty = FeeBook::load(vault);
  ASSERT_TRUE(empty) << empty.error().message;
  EXPECT_EQ(empty.value().last(), nullptr);

  FeeBook book;
  book.relays = {{"relay-east", "QmEast", kRelayAccount}};
  book.rules = rules();
  book.draft = report();
  Round round;
  round.number = 2;
  round.report = report();
  round.rules = rules();
  round.table = computePayouts(round.report, round.rules, 1).value();
  round.digest = roundDigest(round.report, round.rules, round.table);
  book.put(round);
  round.number = 1;
  book.put(round);
  ASSERT_TRUE(book.save(vault));

  auto back = FeeBook::load(vault);
  ASSERT_TRUE(back) << back.error().message;
  EXPECT_EQ(back.value().relays, book.relays);
  EXPECT_EQ(back.value().rules, book.rules);
  ASSERT_TRUE(back.value().draft);
  EXPECT_EQ(back.value().draft->miners.size(), 3u);
  ASSERT_EQ(back.value().rounds.size(), 2u);
  EXPECT_EQ(back.value().last()->number, 2u);
}
