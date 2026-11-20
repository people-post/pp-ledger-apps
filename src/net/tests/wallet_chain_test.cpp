// The wallet's operations from a proven account: who signs, moving everything out.
#include "net/WalletChain.h"

#include "ledger/client/AccountIds.h"
#include "ledger/lib/common/BinaryPack.hpp"
#include "ledger/lib/common/Utilities.h"

#include <gtest/gtest.h>
#include <map>

using namespace apps;
using namespace apps::net;
namespace op = pp::operation;

namespace {

constexpr uint64_t kMine = pp::AccountIds::ID_FIRST_USER + 7;
constexpr uint64_t kSafe = pp::AccountIds::ID_FIRST_USER + 8;
constexpr uint64_t kToken = pp::AccountIds::ID_FIRST_ISSUED + 3;

ChainInfo chain() {
  ChainInfo info;
  info.networkId = "test-net";
  info.genesisHash = std::string(32, '\x04');
  info.meta.config.networkId = "test-net";
  info.meta.config.maxCustomMetaSize = 4096;
  info.meta.config.minFeeCoefficients = {1, 1, 0};
  info.meta.config.maxValidationTimespanSeconds = 86400;
  return info;
}

VerifiedAccount proven(std::map<uint64_t, int64_t> balances) {
  VerifiedAccount a;
  a.id = kMine;
  a.exists = true;
  a.wallet.publicKeys = {pp::utl::mlDsaGenerate().value().publicKey, pp::utl::mlDsaGenerate().value().publicKey};
  a.wallet.minSignatures = 2;
  a.wallet.mBalances = std::move(balances);
  return a;
}

pp::UserAccount stateOf(const VerifiedAccount &a) {
  pp::UserAccount state;
  state.wallet = a.wallet;
  return state;
}

Making making() { return {1'800'000'000, 3600, "evacuation"}; }

} // namespace

TEST(WalletChainTest, TheProofSaysWhoSigns) {
  const auto account = proven({{0, 100}});
  const auto signers = signersOf(account);
  EXPECT_EQ(signers.account, kMine);
  EXPECT_EQ(signers.publicKeys, account.wallet.publicKeys);
  EXPECT_EQ(signers.minSignatures, 2u);
}

TEST(WalletChainTest, MovingEverythingTakesEachTokenAndTheNativeLessFees) {
  // Native only: one transfer of all but its fee.
  auto native = proven({{0, 1000}});
  auto single = makeMoveAll(chain(), signersOf(native), stateOf(native), kSafe, making());
  ASSERT_TRUE(single) << single.error().message;
  EXPECT_EQ(single.value().kind, op::Kind::Transfer);
  auto tx = pp::utl::binaryUnpack<pp::Ledger::TxDefault>(single.value().record.data).value();
  EXPECT_EQ(tx.amount + tx.fee, 1000u);
  EXPECT_EQ(tx.toWalletId, kSafe);

  // A token held, and a token it issues (negative: not moved): a batch, the native last.
  auto mixed = proven({{0, 1000}, {kToken, 40}, {kMine, -500}});
  auto batch = makeMoveAll(chain(), signersOf(mixed), stateOf(mixed), kSafe, making());
  ASSERT_TRUE(batch) << batch.error().message;
  EXPECT_EQ(batch.value().kind, op::Kind::Batch);
  ASSERT_EQ(batch.value().moreRecords.size(), 1u);
  auto first = pp::utl::binaryUnpack<pp::Ledger::TxDefault>(batch.value().record.data).value();
  auto last = pp::utl::binaryUnpack<pp::Ledger::TxDefault>(batch.value().moreRecords[0].data).value();
  EXPECT_EQ(first.tokenId, kToken);
  EXPECT_EQ(first.amount, 40u);
  EXPECT_EQ(last.tokenId, 0u);
  EXPECT_EQ(last.amount + first.fee + last.fee, 1000u);
  EXPECT_EQ(batch.value().signers.minSignatures, 2u);

  // Not enough to pay the fees; not to itself.
  auto poor = proven({{0, 1}, {kToken, 40}});
  EXPECT_FALSE(makeMoveAll(chain(), signersOf(poor), stateOf(poor), kSafe, making()));
  EXPECT_FALSE(makeMoveAll(chain(), signersOf(native), stateOf(native), kMine, making()));
}
