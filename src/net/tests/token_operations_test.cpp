// A token issuer's operations, built against a chain state and checked by pp-ledger's rules.
#include "net/TokenOperations.h"

#include "client/AccountIds.h"
#include "lib/common/BinaryPack.hpp"
#include "lib/common/Utilities.h"

#include <gtest/gtest.h>

using namespace apps::net;
namespace op = pp::operation;

namespace {

constexpr uint64_t kIssuer = pp::AccountIds::ID_FIRST_ISSUED + 50;
constexpr uint64_t kHolder = pp::AccountIds::ID_FIRST_USER + 7;

std::string key() { return pp::utl::mlDsaGenerate().value().publicKey; }

ChainInfo chain() {
  ChainInfo info;
  info.networkId = "test-net";
  info.genesisHash = std::string(32, '\x01');
  auto &c = info.meta.config;
  c.networkId = "test-net";
  c.maxCustomMetaSize = 4096;
  c.freeCustomMetaSize = 0;
  c.minFeeCoefficients = {1, 1, 0};
  c.maxValidationTimespanSeconds = 86400;
  return info;
}

/** The issuer: 2 of 3 keys, `native` to pay with, `issued` of its token out. */
pp::UserAccount issuer(int64_t native, int64_t issued = 0) {
  pp::UserAccount account;
  account.wallet.publicKeys = {key(), key(), key()};
  account.wallet.minSignatures = 2;
  account.wallet.keyType = 1;
  account.wallet.mBalances[pp::AccountIds::ID_GENESIS] = native;
  if (issued != 0) {
    account.wallet.mBalances[kIssuer] = -issued;
  }
  return account;
}

op::SignerSet signersOf(uint64_t id, const pp::UserAccount &account) {
  op::SignerSet signers;
  signers.account = id;
  signers.publicKeys = account.wallet.publicKeys;
  signers.minSignatures = account.wallet.minSignatures;
  return signers;
}

Making making() { return {1'800'000'000, 86400, "as agreed"}; }

pp::Ledger::Record recordOf(const pp::Ledger::TxDefault &tx) {
  pp::Ledger::Record record;
  record.type = pp::Ledger::T_DEFAULT;
  record.data = pp::utl::binaryPack(tx);
  return record;
}

} // namespace

TEST(TokenOperationsTest, TheOverviewReadsTheSupplyAndWatchesTheRenewal) {
  const ChainInfo info = chain();
  pp::UserAccount account = issuer(1'000'000, 2500);
  account.wallet.mBalances[kIssuer + 1] = 40; // another issuer's token, held
  pp::Ledger::TxDefault issued;
  issued.tokenId = kIssuer;
  issued.fromWalletId = kIssuer;
  issued.toWalletId = kHolder;
  issued.amount = 2500;
  issued.fee = 3;
  pp::Ledger::TxDefault funded;
  funded.fromWalletId = pp::AccountIds::ID_REGISTRAR;
  funded.toWalletId = kIssuer;
  funded.amount = 5000;
  auto overview = tokenOverview(info, kIssuer, account, {recordOf(issued), recordOf(funded)});
  ASSERT_TRUE(overview) << overview.error().message;
  EXPECT_EQ(overview.value().supply, 2500u);
  EXPECT_EQ(overview.value().native, 1'000'000);
  EXPECT_GT(overview.value().renewalFee, 0u);
  EXPECT_FALSE(overview.value().low);
  EXPECT_EQ(overview.value().minSignatures, 2u);
  ASSERT_EQ(overview.value().otherTokens.size(), 1u);
  EXPECT_EQ(overview.value().otherTokens[0].first, kIssuer + 1);
  ASSERT_EQ(overview.value().recent.size(), 2u);
  EXPECT_EQ(overview.value().recent[0].what, "Issued 2,500 to account 1,073,741,831");
  EXPECT_EQ(overview.value().recent[0].fee, "3");
  EXPECT_EQ(overview.value().recent[1].what, "Received 5,000 from the registrar");
  EXPECT_EQ(overview.value().recent[1].fee, "");

  // Fewer renewals left than kRenewalsToKeep: it warns.
  const uint64_t fee = overview.value().renewalFee;
  auto low = tokenOverview(info, kIssuer, issuer(static_cast<int64_t>(fee * 2), 2500), {});
  ASSERT_TRUE(low);
  EXPECT_TRUE(low.value().low);

  EXPECT_FALSE(tokenOverview(info, kHolder, account, {})); // a user account issues nothing
}

TEST(TokenOperationsTest, IssuingIsATransferOfTheIssuersOwnToken) {
  const ChainInfo info = chain();
  const pp::UserAccount account = issuer(1000);
  const auto signers = signersOf(kIssuer, account);
  auto issue = makeIssue(info, signers, account, kHolder, 700, 10, making());
  ASSERT_TRUE(issue) << issue.error().message;
  EXPECT_EQ(issue.value().kind, op::Kind::Transfer);
  auto tx = pp::utl::binaryUnpack<pp::Ledger::TxDefault>(issue.value().record.data);
  ASSERT_TRUE(tx);
  EXPECT_EQ(tx.value().tokenId, kIssuer);
  EXPECT_EQ(tx.value().fromWalletId, kIssuer);
  EXPECT_EQ(tx.value().toWalletId, kHolder);
  EXPECT_EQ(tx.value().amount, 700u);
  EXPECT_EQ(issue.value().signers.minSignatures, 2u);

  EXPECT_FALSE(makeIssue(info, signers, account, kIssuer, 700, 10, making())); // to itself
  EXPECT_FALSE(makeIssue(info, signers, account, kHolder, 0, 10, making()));
  EXPECT_FALSE(makeIssue(info, signers, account, kHolder, 700, 0, making()));     // below the least fee
  EXPECT_FALSE(makeIssue(info, signers, issuer(5), kHolder, 700, 10, making())); // cannot pay the fee
  EXPECT_FALSE(makeIssue(info, signersOf(kHolder, account), account, kIssuer, 700, 10, making())); // not an issuer
  Making late = making();
  late.validForSeconds = 2 * 86400;
  EXPECT_FALSE(makeIssue(info, signers, account, kHolder, 700, 10, late)); // past the chain's window
}

TEST(TokenOperationsTest, SendingStaysWithinWhatTheAccountHolds) {
  const ChainInfo info = chain();
  pp::UserAccount account = issuer(1000);
  account.wallet.mBalances[kIssuer + 1] = 40;
  const auto signers = signersOf(kIssuer, account);
  auto native = makeSend(info, signers, account, kHolder, 0, 900, 10, making());
  ASSERT_TRUE(native) << native.error().message;
  EXPECT_FALSE(makeSend(info, signers, account, kHolder, 0, 995, 10, making())); // amount and fee
  EXPECT_TRUE(makeSend(info, signers, account, kHolder, kIssuer + 1, 40, 10, making()));
  EXPECT_FALSE(makeSend(info, signers, account, kHolder, kIssuer + 1, 41, 10, making()));
  EXPECT_FALSE(makeSend(info, signers, account, kHolder, kIssuer + 2, 1, 10, making())); // not held
  // Its own token: issued, whatever it holds of it.
  auto own = makeSend(info, signers, account, kHolder, kIssuer, 5000, 10, making());
  ASSERT_TRUE(own) << own.error().message;
  EXPECT_EQ(pp::utl::binaryUnpack<pp::Ledger::TxDefault>(own.value().record.data).value().tokenId, kIssuer);
}

TEST(TokenOperationsTest, AKeyReplacementIsSignedByTheKeysInForce) {
  const ChainInfo info = chain();
  const pp::UserAccount account = issuer(1000, 300);
  const auto signers = signersOf(kIssuer, account);
  const std::vector<std::string> next = {key(), key()};
  auto least = keyReplacementFee(info, signers, account, next, 2);
  ASSERT_TRUE(least) << least.error().message;
  EXPECT_GT(least.value(), 0u);
  auto replaced = makeKeyReplacement(info, signers, account, next, 2, least.value(), making());
  ASSERT_TRUE(replaced) << replaced.error().message;
  EXPECT_EQ(replaced.value().kind, op::Kind::AccountUpdate);
  EXPECT_EQ(replaced.value().signers.publicKeys, account.wallet.publicKeys); // the keys in force sign
  auto tx = pp::utl::binaryUnpack<pp::Ledger::TxUserUpdate>(replaced.value().record.data);
  ASSERT_TRUE(tx);
  pp::UserAccount after;
  ASSERT_TRUE(after.ltsFromString(tx.value().meta));
  EXPECT_EQ(after.wallet.publicKeys, next);
  EXPECT_EQ(after.wallet.minSignatures, 2u);
  EXPECT_EQ(after.wallet.mBalances.at(kIssuer), -300); // the rest as it is
  auto summary = op::summarize(replaced.value());
  ASSERT_TRUE(summary) << summary.error().message;

  EXPECT_FALSE(makeKeyReplacement(info, signers, account, next, 2, least.value() - 1, making()));
  EXPECT_FALSE(makeKeyReplacement(info, signers, account, next, 3, least.value(), making()));
  EXPECT_FALSE(makeKeyReplacement(info, signers, account, {}, 1, least.value(), making()));
  EXPECT_FALSE(makeKeyReplacement(info, signers, account, account.wallet.publicKeys, 2, least.value(), making()));
  EXPECT_FALSE(makeKeyReplacement(info, signers, issuer(0), next, 2, least.value(), making())); // cannot pay
}

TEST(TokenOperationsTest, AnAccountNamesARecoverySetItsSetAnnouncesAndTheAccountCancels) {
  ChainInfo info = chain();
  info.meta.config.slotDuration = 5;
  info.meta.config.slotsPerEpoch = 720; // an hour
  auto days = delayEpochsFor(info, 7);
  ASSERT_TRUE(days) << days.error().message;
  EXPECT_EQ(days.value(), 7u * 24);
  EXPECT_FALSE(delayEpochsFor(info, 0));
  EXPECT_FALSE(delayEpochsFor(chain(), 7)); // no epoch length

  pp::UserAccount account = issuer(1000, 300);
  const auto signers = signersOf(kIssuer, account);
  const std::vector<std::string> guardians = {key(), key(), key()};
  auto set = makeRecoverySet(info, signers, account, guardians, 2, days.value(), making());
  ASSERT_TRUE(set) << set.error().message;
  EXPECT_EQ(set.value().kind, op::Kind::AccountUpdate);
  EXPECT_EQ(set.value().signers.publicKeys, account.wallet.publicKeys); // the keys in force sign
  pp::UserAccount after;
  ASSERT_TRUE(after.ltsFromString(
      pp::utl::binaryUnpack<pp::Ledger::TxUserUpdate>(set.value().record.data).value().meta));
  EXPECT_EQ(after.wallet.recovery.publicKeys, guardians);
  EXPECT_EQ(after.wallet.recovery.minSignatures, 2u);
  EXPECT_EQ(after.wallet.recovery.delayEpochs, 168u);
  EXPECT_EQ(after.wallet.publicKeys, account.wallet.publicKeys); // its keys stay
  EXPECT_FALSE(makeRecoverySet(info, signers, account, guardians, 4, 1, making()));
  EXPECT_FALSE(makeRecoverySet(info, signers, account, {account.wallet.publicKeys[0]}, 1, 1, making())); // its own
  EXPECT_FALSE(makeRecoverySet(info, signers, account, {}, 0, 0, making())); // none to remove
  EXPECT_FALSE(makeRecoverySet(info, signers, issuer(0), guardians, 2, 1, making())); // cannot pay
  EXPECT_FALSE(makeRecoverySet(info, signersOf(pp::AccountIds::ID_RESERVE, account), account, guardians, 2, 1,
                               making())); // a system account

  // Its recovery set announces the next keys, for the account.
  EXPECT_FALSE(makeRecoveryAnnouncement(info, kIssuer, account, {key()}, 1, making())); // no set yet
  account.wallet.recovery = after.wallet.recovery;
  const std::vector<std::string> next = {key()};
  auto announce = makeRecoveryAnnouncement(info, kIssuer, account, next, 1, making());
  ASSERT_TRUE(announce) << announce.error().message;
  EXPECT_EQ(announce.value().kind, op::Kind::AccountRecovery);
  EXPECT_EQ(announce.value().signers.publicKeys, guardians);
  EXPECT_EQ(announce.value().signers.minSignatures, 2u);
  EXPECT_EQ(announce.value().signers.account, kIssuer);
  EXPECT_FALSE(makeRecoveryAnnouncement(info, kIssuer, account, next, 2, making())); // 2 of 1

  // Announced: once at a time; the account's keys cancel it.
  EXPECT_FALSE(makeRecoveryCancel(info, signers, account, making())); // nothing announced
  account.wallet.recovery.pending = true;
  account.wallet.recovery.nextPublicKeys = next;
  account.wallet.recovery.nextMinSignatures = 1;
  account.wallet.recovery.applyEpoch = 900;
  EXPECT_FALSE(makeRecoveryAnnouncement(info, kIssuer, account, next, 1, making()));
  auto cancel = makeRecoveryCancel(info, signers, account, making());
  ASSERT_TRUE(cancel) << cancel.error().message;
  pp::UserAccount cancelled;
  ASSERT_TRUE(cancelled.ltsFromString(
      pp::utl::binaryUnpack<pp::Ledger::TxUserUpdate>(cancel.value().record.data).value().meta));
  EXPECT_FALSE(cancelled.wallet.recovery.pending);
  EXPECT_EQ(cancelled.wallet.recovery.publicKeys, guardians);
  // A key replacement cancels it too: the account's update never carries an announcement.
  auto least = keyReplacementFee(info, signers, account, next, 1);
  ASSERT_TRUE(least);
  auto replaced = makeKeyReplacement(info, signers, account, next, 1, least.value(), making());
  ASSERT_TRUE(replaced) << replaced.error().message;
  pp::UserAccount replacedState;
  ASSERT_TRUE(replacedState.ltsFromString(
      pp::utl::binaryUnpack<pp::Ledger::TxUserUpdate>(replaced.value().record.data).value().meta));
  EXPECT_FALSE(replacedState.wallet.recovery.pending);
}
