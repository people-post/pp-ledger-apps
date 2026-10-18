// The admin set's operations, built against a chain state and checked by pp-ledger's rules.
#include "net/AdminOperations.h"

#include "client/AccountIds.h"
#include "lib/common/BinaryPack.hpp"
#include "lib/common/Utilities.h"

#include <gtest/gtest.h>

using namespace admin::net;
namespace op = pp::operation;

namespace {

std::string key() { return pp::utl::mlDsaGenerate().value().publicKey; }

/** A chain as a node would show it: 2 of 3 admins, a recovery set, beacon generation 1. */
ChainInfo chain() {
  ChainInfo info;
  info.networkId = "test-net";
  info.genesisHash = std::string(32, '\x01');
  auto &c = info.meta.config;
  c.networkId = "test-net";
  c.genesisTime = 1'700'000'000;
  c.slotDuration = 7;
  c.slotsPerEpoch = 1800;
  c.maxTransactionsPerBlock = 4096;
  c.maxBlockBytes = 4u << 20;
  c.maxCustomMetaSize = 4096;
  c.minFeeCoefficients = {1, 1, 0};
  c.maxValidationTimespanSeconds = 86400;
  c.maxIssuancePerEpoch = 1'000'000;
  c.heartbeatSlots = 100;
  info.meta.genesis.wallet.publicKeys = {key(), key(), key()};
  info.meta.genesis.wallet.minSignatures = 2;
  info.meta.genesis.wallet.keyType = 1;
  info.meta.authority.recovery.publicKeys = {key(), key(), key()};
  info.meta.authority.recovery.minSignatures = 2;
  info.meta.authority.recovery.delayEpochs = 10;
  info.meta.authority.beacon.networkId = "test-net";
  info.meta.authority.beacon.generation = 1;
  info.meta.authority.beacon.publicKey = key();
  info.meta.authority.beacon.cutoverHeight = 0;
  return info;
}

Making making() { return {1'800'000'000, 86400, "as agreed"}; }

} // namespace

TEST(AdminOperationsTest, AMintGoesToTheReserveWithinTheCap) {
  const ChainInfo info = chain();
  EXPECT_FALSE(makeMint(info, 0, 1, making()));
  EXPECT_FALSE(makeMint(info, 1'000'001, 1, making())); // above the cap per epoch
  EXPECT_FALSE(makeMint(info, 1'000'000, 1, making())); // the fee counts too
  ChainInfo none = info;
  none.meta.config.maxIssuancePerEpoch = 0;
  EXPECT_FALSE(makeMint(none, 1000, 1, making())); // 0 mints nothing
  EXPECT_FALSE(makeMint(info, 1000, 0, making()));       // below the least fee
  Making tooLong = making();
  tooLong.validForSeconds = 2 * 86400;
  EXPECT_FALSE(makeMint(info, 1000, 1, tooLong));
  auto mint = makeMint(info, 999'999, 1, making());
  ASSERT_TRUE(mint) << mint.error().message;
  EXPECT_EQ(mint.value().kind, op::Kind::Mint);
  EXPECT_EQ(mint.value().signers.account, pp::AccountIds::ID_GENESIS);
  EXPECT_EQ(mint.value().signers.publicKeys, info.meta.genesis.wallet.publicKeys);
  EXPECT_EQ(mint.value().genesisHash, info.genesisHash);
}

TEST(AdminOperationsTest, AConfigUpdateChangesSettingsTheChainAllows) {
  const ChainInfo info = chain();
  pp::BlockChainConfig next = info.meta.config;
  EXPECT_FALSE(makeConfigUpdate(info, next, std::nullopt, making())); // nothing changes

  next.slotDuration = 5;
  EXPECT_FALSE(makeConfigUpdate(info, next, std::nullopt, making())); // fixed at genesis
  next = info.meta.config;
  next.maxBlockBytes = 8u << 20;
  EXPECT_FALSE(makeConfigUpdate(info, next, std::nullopt, making())); // above what a message carries

  next = info.meta.config;
  next.maxIssuancePerEpoch = 2'000'000;
  next.minFeeCoefficients = {2, 1, 0};
  auto update = makeConfigUpdate(info, next, std::nullopt, making());
  ASSERT_TRUE(update) << update.error().message;
  EXPECT_EQ(update.value().kind, op::Kind::ConfigUpdate);
  const auto changes = configChanges(info.meta.config, next);
  ASSERT_EQ(changes.size(), 2u);
  EXPECT_EQ(changes[0].title, "Mint per epoch, most");
  EXPECT_EQ(changes[0].before, "1,000,000");
  EXPECT_EQ(changes[0].after, "2,000,000");
  // The record carries the whole next meta: the authority unchanged, nothing pending.
  auto tx = pp::utl::binaryUnpack<pp::Ledger::TxConfig>(update.value().record.data).value();
  pp::GenesisAccountMeta carried;
  ASSERT_TRUE(carried.ltsFromString(tx.meta));
  EXPECT_EQ(carried.config.maxIssuancePerEpoch, 2'000'000u);
  EXPECT_EQ(carried.authority, info.meta.authority);
  EXPECT_FALSE(carried.hasPending);
  EXPECT_EQ(tx.fee, 0u);
  EXPECT_TRUE(op::summarize(update.value()));

  ChainInfo pending = info;
  pending.meta.hasPending = true;
  pending.meta.pending.activationEpoch = 7;
  EXPECT_FALSE(makeConfigUpdate(pending, next, std::nullopt, making())); // one at a time
}

TEST(AdminOperationsTest, RotatingTheAdminSetKeepsItsShape) {
  const ChainInfo info = chain();
  pp::Wallet two;
  two.publicKeys = {key(), key()};
  two.minSignatures = 2;
  EXPECT_FALSE(makeConfigUpdate(info, info.meta.config, two, making())); // at least 3 keys
  pp::Wallet fresh;
  fresh.publicKeys = {key(), key(), key(), key()};
  fresh.minSignatures = 3;
  auto rotation = makeConfigUpdate(info, info.meta.config, fresh, making());
  ASSERT_TRUE(rotation) << rotation.error().message;
  // Signed by the admin set in force; carries the next one.
  EXPECT_EQ(rotation.value().signers.publicKeys, info.meta.genesis.wallet.publicKeys);
  auto tx = pp::utl::binaryUnpack<pp::Ledger::TxConfig>(rotation.value().record.data).value();
  pp::GenesisAccountMeta carried;
  ASSERT_TRUE(carried.ltsFromString(tx.meta));
  EXPECT_EQ(carried.genesis.wallet.publicKeys, fresh.publicKeys);
  EXPECT_EQ(carried.genesis.wallet.minSignatures, 3u);
}

TEST(AdminOperationsTest, TheNextBeaconCertificateIsTheNextGeneration) {
  const ChainInfo info = chain();
  // An admin key cannot be the beacon's.
  EXPECT_FALSE(makeBeaconCertificate(info, info.meta.genesis.wallet.publicKeys[0], {}, making()));
  const std::string beacon = key();
  auto cert = makeBeaconCertificate(info, beacon, {"/dns4/beacon.example/udp/8517"}, making());
  ASSERT_TRUE(cert) << cert.error().message;
  EXPECT_EQ(cert.value().kind, op::Kind::BeaconCertificate);
  auto tx = pp::utl::binaryUnpack<pp::Ledger::TxBeaconCert>(cert.value().record.data).value();
  pp::BeaconCertificate carried;
  ASSERT_TRUE(carried.ltsFromString(tx.meta));
  EXPECT_EQ(carried.generation, 2u);
  EXPECT_EQ(carried.publicKey, beacon);
  EXPECT_FALSE(carried.hasCutover());

  ChainInfo frozen = info;
  frozen.meta.authority.hasPendingRecovery = true;
  frozen.meta.authority.pendingRecovery.applyEpoch = 99;
  EXPECT_FALSE(makeBeaconCertificate(frozen, beacon, {}, making())); // a recovery pending
}
