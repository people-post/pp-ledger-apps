#pragma once
// The fee app's chain work (projects/fee-app/DESIGN.md): a round's period read from the chain — the
// producers' blocks, fees and misses (PRODUCER_STATS), their registration, the fee account's balance —
// and the round's payouts made into one operation from the fee account.

#include "net/AdminOperations.h"
#include "core/FeeRounds.h"

#include <cstdint>
#include <functional>

namespace apps::net {

/**
 * The block a period ending at `unixTime` stops before: the first block whose slot starts at or after
 * it (the chain's tip when none does). The period is [its start, this block).
 */
pp::Roe<uint64_t> blockAt(ChainLink &link, const ChainInfo &chain, int64_t unixTime);

/**
 * The report of blocks [from, to) from the chain: PRODUCER_STATS page by page (`progress` gets the
 * blocks read so far and the period's length), each producer's registration now, the fee account's
 * balance now. Relays are the holders' to add (FD004).
 */
pp::Roe<fees::Report> readPeriod(ChainLink &link, uint64_t from, uint64_t to,
                                 const std::function<void(uint64_t, uint64_t)> &progress);

/** The least fee of a payout carrying `meta`, under the config in force. */
pp::Roe<uint64_t> payoutFee(const ChainInfo &chain, const std::string &meta);

/**
 * The round's payouts from the fee account (`feeAccount`: its signers), each carrying roundMeta: a
 * batch, or a transfer when it pays one account. Refused when they and their fees pass
 * `balanceNow`, or a fee is below the least.
 */
pp::Roe<pp::operation::Operation> makePayouts(const ChainInfo &chain, const pp::operation::SignerSet &feeAccount,
                                              uint64_t balanceNow, const fees::Round &round, const Making &making);

} // namespace apps::net
