#pragma once
// The fee app's rounds (projects/fee-app/DESIGN.md): what a period's report holds, the rules that
// split the fee account's balance, the payout table they make, and the round file that keeps it all.
// Pure: no network, no clock — the same inputs make the same table and digest on every machine.

#include "core/Vault.h"
#include "common/Error.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace apps::fees {

/** Basis points: 10,000 is 100%. */
inline constexpr uint32_t kWhole = 10000;

/** "12.5" or "12.5%" → 1250 basis points (two decimals at most, 0 to 100). */
pp::Roe<uint32_t> parsePercent(const std::string &text);
/** 1250 → "12.5%". */
std::string formatPercent(uint32_t basisPoints);

/** A round's blocks: [fromBlock, toBlock). */
struct Period {
  uint64_t fromBlock{0};
  uint64_t toBlock{0};
  /** Slots of its first and last block (0 when it has none). */
  uint64_t firstSlot{0};
  uint64_t lastSlot{0};
};

/** One miner's part of the period, from the chain: PRODUCER_STATS and its account at the end. */
struct MinerLine {
  uint64_t account{0};
  uint64_t blocks{0};
  uint64_t fees{0};
  /** Slots it led while due and skipped (WIRE_SCHEMA § Producer statistics). */
  uint64_t misses{0};
  /** A registered miner when the report was read. */
  bool registered{false};
};

/** A relay the holders pay (FD004): its name, its node's peer id, the account it is paid to. */
struct Relay {
  std::string name;
  std::string peerId;
  uint64_t account{0};

  bool operator==(const Relay &) const = default;
};

/** A relay's measure this round, entered by the holders: uptime in basis points. */
struct RelayLine {
  Relay relay;
  uint32_t uptime{0};
};

/** What a round's payouts are computed from (step 1). */
struct Report {
  Period period;
  uint64_t blocks{0};
  uint64_t fees{0};
  std::vector<MinerLine> miners;
  /** The fee account's native balance when the report was read: what there is to hand on. */
  uint64_t feeBalance{0};
  /** From outside the chain (FD004): kept apart, with where they come from and who entered them. */
  std::vector<RelayLine> relays;
  std::string relaySource;
  std::string relaysEnteredBy;
};

/**
 * How a round splits the fee account's balance (FD005). The pot is the balance less what is kept
 * back and the batch's fees; the shares split it, each among its recipients by one measure.
 */
struct Rules {
  std::string name;
  uint32_t keepBack{0};       ///< of the balance, basis points
  uint32_t minersByBlocks{0}; ///< of the pot: miners, by blocks produced less missPenalty a miss
  uint32_t minersByFees{0};   ///< miners, by the fees in their blocks
  uint32_t relaysByUptime{0}; ///< relays, by uptime
  uint32_t everyoneEqual{0};  ///< every miner and relay paid this round, equally
  /** Blocks a miss costs in minersByBlocks (a miner's count floors at 0). */
  uint64_t missPenalty{0};
  /** Payouts below it are not made: the amount stays in the fee account. */
  uint64_t minPayout{0};
  /** Leave out miners no longer registered when the report was read. */
  bool registeredOnly{true};

  bool operator==(const Rules &) const = default;
};

/** The shares add up to 100% and the keep-back is at most 100%. */
pp::Roe<void> check(const Rules &rules);
/** Their identity: SHA-256 (hex) of their JSON — a round names the rules it used by it. */
std::string rulesHash(const Rules &rules);

/** The shares in order, for tables: minersByBlocks, minersByFees, relaysByUptime, everyoneEqual. */
inline constexpr size_t kShares = 4;
std::vector<std::string> shareTitles();

struct Payout {
  uint64_t account{0};
  /** "miner", a relay's name, or both. */
  std::string who;
  uint64_t amount{0};
  /** What each share gave it (shareTitles order). */
  std::vector<uint64_t> byShare;
};

/** The payouts a report and rules make (step 2). */
struct Table {
  uint64_t keptBack{0};
  /** What the shares split: the balance less keptBack and a fee for each possible recipient. */
  uint64_t pot{0};
  uint64_t feeEach{0};
  /** By account. */
  std::vector<Payout> payouts;
  uint64_t total{0};
  /** Fees of the payouts made. */
  uint64_t fees{0};
  /** What stays in the fee account: the balance less total and fees. */
  uint64_t left{0};
};

/** Each share's amount split by its measure, rounded down; what rounding leaves stays. */
pp::Roe<Table> computePayouts(const Report &report, const Rules &rules, uint64_t feeEach);

/** A round (steps 3 and 4): its number, report, rules and table, and the digest that names them. */
struct Round {
  uint64_t number{0};
  Report report;
  Rules rules;
  Table table;
  /** SHA-256 (hex) of the report, rules and table (roundDigest): every transfer carries it. */
  std::string digest;
  /** When it was made (Unix seconds), and the batch operation's file name. */
  int64_t madeAt{0};
  std::string operationFile;
};

std::string roundDigest(const Report &report, const Rules &rules, const Table &table);

/** The metadata each transfer of the round carries (FD007): "pp-fee-round/1 <number> <toBlock> <digest>". */
std::string roundMeta(const Round &round);
struct RoundMark {
  uint64_t number{0};
  uint64_t toBlock{0};
  std::string digest;
};
std::optional<RoundMark> parseRoundMeta(const std::string &meta);

/** The round file: format pp-ledger-fee-round, version 1. Reading checks the digest. */
inline constexpr const char *kRoundFormat = "pp-ledger-fee-round";
std::string roundToJson(const Round &round);
pp::Roe<Round> roundFromJson(const std::string &json);

/** The fee app's own state, in the vault: the relays, the rules in force, the draft round, the rounds made. */
struct FeeBook {
  static constexpr const char *kFileName = "fees.json";

  std::vector<Relay> relays;
  std::optional<Rules> rules;
  /** The round being prepared: its report so far. */
  std::optional<Report> draft;
  /** By number. */
  std::vector<Round> rounds;

  static pp::Roe<FeeBook> load(const Vault &vault);
  pp::Roe<void> save(Vault &vault) const;
  /** Add `round`, or replace the round of its number (an imported copy); kept by number. */
  void put(const Round &round);
  /** The latest round, if any: the next one starts where it ended. */
  const Round *last() const;
};

} // namespace apps::fees
