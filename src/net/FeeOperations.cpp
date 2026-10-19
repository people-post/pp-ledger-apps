#include "net/FeeOperations.h"

#include "ledger/chain/RecordHandler.h"
#include "ledger/client/AccountIds.h"

#include <map>

namespace apps::net {

namespace op = pp::operation;

pp::Roe<uint64_t> blockAt(ChainLink &link, const ChainInfo &chain, int64_t unixTime) {
  const auto &config = chain.meta.config;
  if (config.slotDuration == 0) {
    return pp::Error("The chain's config names no slot duration");
  }
  // The first slot starting at or after the time.
  const int64_t since = unixTime - config.genesisTime;
  const uint64_t slot = since <= 0 ? 0 : (static_cast<uint64_t>(since) + config.slotDuration - 1) / config.slotDuration;
  uint64_t lo = 1;
  uint64_t hi = chain.nextBlockId;
  while (lo < hi) {
    const uint64_t mid = lo + (hi - lo) / 2;
    auto block = link.block(mid);
    if (!block) {
      return block.error();
    }
    if (block.value().block.slot < slot) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo;
}

pp::Roe<fees::Report> readPeriod(ChainLink &link, uint64_t from, uint64_t to,
                                 const std::function<void(uint64_t, uint64_t)> &progress) {
  if (from == 0 || to <= from) {
    return pp::Error("The period has no blocks: it ends where it starts");
  }
  fees::Report report;
  report.period.fromBlock = from;
  report.period.toBlock = to;
  std::map<uint64_t, fees::MinerLine> miners;
  uint64_t next = from;
  while (next < to) {
    auto page = link.producerStats(next, to);
    if (!page) {
      return page.error();
    }
    const auto &p = page.value();
    if (p.blocks > 0) {
      if (report.blocks == 0) {
        report.period.firstSlot = p.firstSlot;
      }
      report.period.lastSlot = p.lastSlot;
    }
    report.blocks += p.blocks;
    report.fees += p.fees;
    for (const auto &s : p.producers) {
      auto &m = miners[s.accountId];
      m.account = s.accountId;
      m.blocks += s.blocks;
      m.fees += s.fees;
      m.misses += s.missesCharged;
    }
    if (p.nextBlockId <= next) {
      return pp::Error("The node holds blocks up to " + std::to_string(next) + " only: the period ends after its tip");
    }
    next = p.nextBlockId;
    if (progress) {
      progress(next - from, to - from);
    }
  }
  for (auto &[account, m] : miners) {
    auto state = link.account(account);
    m.registered = state && state.value().wallet.miner.registered; // an ended account is not
    report.miners.push_back(m);
  }
  auto fee = link.account(pp::AccountIds::ID_FEE);
  if (!fee) {
    return fee.error();
  }
  const auto &balances = fee.value().wallet.mBalances;
  const auto native = balances.find(pp::AccountIds::ID_GENESIS);
  report.feeBalance = native == balances.end() || native->second < 0 ? 0 : static_cast<uint64_t>(native->second);
  return report;
}

pp::Roe<uint64_t> payoutFee(const ChainInfo &chain, const std::string &meta) {
  pp::Ledger::TxDefault probe;
  probe.fromWalletId = pp::AccountIds::ID_FEE;
  probe.meta = meta;
  auto fee = pp::minimumFeeFor(chain.meta.config, pp::Ledger::TypedTx(probe));
  if (!fee) {
    return pp::Error("Cannot work out the payouts' fee: " + fee.error().message);
  }
  return fee.value();
}

pp::Roe<op::Operation> makePayouts(const ChainInfo &chain, const op::SignerSet &feeAccount, uint64_t balanceNow,
                                   const fees::Round &round, const Making &making) {
  const uint64_t longest = chain.meta.config.maxValidationTimespanSeconds;
  if (making.validForSeconds <= 0 || (longest > 0 && static_cast<uint64_t>(making.validForSeconds) > longest)) {
    return pp::Error("The chain lets an operation wait at most " + std::to_string(longest / 3600) + " hours");
  }
  if (feeAccount.account != pp::AccountIds::ID_FEE) {
    return pp::Error("Payouts come from the fee account");
  }
  const auto &payouts = round.table.payouts;
  if (payouts.empty()) {
    return pp::Error("The round pays nobody");
  }
  if (payouts.size() > op::kMaxBatchTransfers) {
    return pp::Error("A round pays at most " + std::to_string(op::kMaxBatchTransfers) + " accounts");
  }
  if (round.digest != fees::roundDigest(round.report, round.rules, round.table)) {
    return pp::Error("The round does not match its digest");
  }
  const std::string meta = fees::roundMeta(round);
  auto least = payoutFee(chain, meta);
  if (!least) {
    return least.error();
  }
  if (round.table.feeEach < least.value()) {
    return pp::Error("Each payout's fee must be at least " + op::groupDigits(least.value()) +
                     ": work the payouts out again");
  }
  if (balanceNow < round.table.total + round.table.fees) {
    return pp::Error("The fee account holds " + op::groupDigits(balanceNow) + " now: not enough for " +
                     op::groupDigits(round.table.total) + " and " + op::groupDigits(round.table.fees) + " in fees");
  }
  op::Common common;
  common.networkId = chain.networkId;
  common.genesisHash = chain.genesisHash;
  common.signers = feeAccount;
  common.window = op::windowFrom(making.now, making.validForSeconds);
  common.notes = making.notes;
  common.createdAt = making.now;
  if (payouts.size() == 1) {
    return op::transfer(common, payouts.front().account, payouts.front().amount, round.table.feeEach, 0, meta);
  }
  std::vector<op::Payment> payments;
  for (const auto &p : payouts) {
    payments.push_back({p.account, p.amount, 0});
  }
  return op::transfers(common, payments, round.table.feeEach, meta);
}

} // namespace apps::net
