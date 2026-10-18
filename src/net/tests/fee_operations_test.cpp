// The fee app's payouts: a round made into one operation from the fee account, each transfer
// carrying the round's mark.
#include "net/FeeOperations.h"

#include "client/AccountIds.h"
#include "lib/common/BinaryPack.hpp"
#include "lib/common/Utilities.h"

#include <gtest/gtest.h>

using namespace apps;
using namespace apps::net;
namespace op = pp::operation;

namespace {

std::string key() { return pp::utl::mlDsaGenerate().value().publicKey; }

ChainInfo chain() {
  ChainInfo info;
  info.networkId = "test-net";
  info.genesisHash = std::string(32, '\x02');
  info.meta.config.networkId = "test-net";
  info.meta.config.maxCustomMetaSize = 4096;
  info.meta.config.freeCustomMetaSize = 32;
  info.meta.config.minFeeCoefficients = {1, 1, 0};
  info.meta.config.maxValidationTimespanSeconds = 86400;
  return info;
}

op::SignerSet feeHolders() {
  op::SignerSet s;
  s.account = pp::AccountIds::ID_FEE;
  s.publicKeys = {key(), key(), key()};
  s.minSignatures = 2;
  return s;
}

/** A round of `payees` miners, its fee each the least for its metadata. */
fees::Round round(size_t payees) {
  fees::Round r;
  r.number = 4;
  r.report.period = {10, 500, 20, 990};
  r.report.feeBalance = 1'000'000;
  for (size_t i = 0; i < payees; ++i) {
    r.report.miners.push_back({pp::AccountIds::ID_FIRST_ISSUED + i, 10 + i, 0, 0, true});
  }
  r.rules.minersByBlocks = fees::kWhole;
  // The metadata's length does not depend on the digest: its fee is known before the table.
  fees::Round sample = r;
  sample.digest = std::string(64, '0');
  const uint64_t feeEach = payoutFee(chain(), fees::roundMeta(sample)).value();
  r.table = fees::computePayouts(r.report, r.rules, feeEach).value();
  r.digest = fees::roundDigest(r.report, r.rules, r.table);
  return r;
}

Making making() { return {1'800'000'000, 86400, "round 4"}; }

} // namespace

TEST(FeeOperationsTest, ARoundIsOneBatchFromTheFeeAccount) {
  const fees::Round r = round(3);
  auto made = makePayouts(chain(), feeHolders(), 1'000'000, r, making());
  ASSERT_TRUE(made) << made.error().message;
  EXPECT_EQ(made.value().kind, op::Kind::Batch);
  ASSERT_EQ(made.value().moreRecords.size(), 2u);
  uint64_t total = 0;
  for (const auto *record : {&made.value().record, &made.value().moreRecords[0], &made.value().moreRecords[1]}) {
    auto tx = pp::utl::binaryUnpack<pp::Ledger::TxDefault>(record->data);
    ASSERT_TRUE(tx);
    EXPECT_EQ(tx.value().fromWalletId, pp::AccountIds::ID_FEE);
    EXPECT_EQ(tx.value().fee, r.table.feeEach);
    auto mark = fees::parseRoundMeta(tx.value().meta);
    ASSERT_TRUE(mark) << tx.value().meta;
    EXPECT_EQ(mark->number, 4u);
    EXPECT_EQ(mark->toBlock, 500u);
    EXPECT_EQ(mark->digest, r.digest);
    total += tx.value().amount;
  }
  EXPECT_EQ(total, r.table.total);
  EXPECT_GT(r.table.feeEach, 1u) << "the metadata past the free size costs";
}

TEST(FeeOperationsTest, OnePayoutIsATransferAndTheBalanceMustCoverAll) {
  auto single = makePayouts(chain(), feeHolders(), 1'000'000, round(1), making());
  ASSERT_TRUE(single) << single.error().message;
  EXPECT_EQ(single.value().kind, op::Kind::Transfer);
  EXPECT_TRUE(fees::parseRoundMeta(pp::utl::binaryUnpack<pp::Ledger::TxDefault>(single.value().record.data).value().meta));

  const fees::Round r = round(3);
  EXPECT_FALSE(makePayouts(chain(), feeHolders(), r.table.total + r.table.fees - 1, r, making())); // spent meanwhile
  fees::Round changed = r;
  changed.table.payouts[0].amount += 1;
  EXPECT_FALSE(makePayouts(chain(), feeHolders(), 1'000'000, changed, making())); // not its digest
  fees::Round cheap = r;
  cheap.table = fees::computePayouts(cheap.report, cheap.rules, 1).value();
  cheap.digest = fees::roundDigest(cheap.report, cheap.rules, cheap.table);
  EXPECT_FALSE(makePayouts(chain(), feeHolders(), 1'000'000, cheap, making())); // fee below the least
  op::SignerSet reserve = feeHolders();
  reserve.account = pp::AccountIds::ID_RESERVE;
  EXPECT_FALSE(makePayouts(chain(), reserve, 1'000'000, r, making()));
}
