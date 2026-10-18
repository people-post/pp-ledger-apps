// The operation workflow: open for review, sign with vault keys, merge — on pp-ledger's operation
// files (OPERATION_FILE.md).
#include "core/Operations.h"
#include "TempDir.h"

#include "client/AccountIds.h"

#include <gtest/gtest.h>

using namespace admin;
namespace op = pp::operation;

namespace {

struct Holders {
  test::TempDir dir;
  Vault alice{dir / "alice", KdfLimits::minimum()};
  Vault bob{dir / "bob", KdfLimits::minimum()};
  KeyInfo aliceKey, bobKey, carolKey;

  Holders() {
    EXPECT_TRUE(alice.create("alice's passphrase"));
    EXPECT_TRUE(bob.create("bob's passphrase"));
    aliceKey = alice.generateKey("Alice", Role::Reserve).value();
    bobKey = bob.generateKey("Bob", Role::Reserve).value();
    carolKey = bob.generateKey("Carol (not a signer)", Role::Reserve).value();
  }

  /** A reserve transfer, 2 of Alice and Bob, written to `name`. */
  std::string transferFile(const std::string &name) {
    op::Common common;
    common.networkId = "test-net";
    common.signers.account = pp::AccountIds::ID_RESERVE;
    common.signers.minSignatures = 2;
    common.signers.publicKeys = {aliceKey.publicKey, bobKey.publicKey};
    common.window = op::windowFrom(1'800'000'000, 86400);
    auto transfer = op::transfer(common, pp::AccountIds::ID_REGISTRAR, 1'000'000, 1);
    EXPECT_TRUE(transfer);
    EXPECT_TRUE(op::save(transfer.value(), dir / name));
    return dir / name;
  }
};

} // namespace

TEST(OperationsTest, SignersSignTheirCopiesWithVaultKeysAndTheCopiesMerge) {
  Holders h;
  const std::string file = h.transferFile("transfer.json");

  auto forAlice = openOperation(file);
  ASSERT_TRUE(forAlice) << forAlice.error().message;
  EXPECT_EQ(forAlice.value().summary.title, "Transfer");
  ASSERT_EQ(keysToSign(forAlice.value().operation, h.alice.keys().value()).size(), 1u);
  ASSERT_TRUE(signWithVault(h.alice, h.aliceKey.id, forAlice.value().operation));
  EXPECT_TRUE(keysToSign(forAlice.value().operation, h.alice.keys().value()).empty());
  const std::string aliceCopy = signedCopyPath(file, h.aliceKey.publicKey);
  ASSERT_TRUE(op::save(forAlice.value().operation, aliceCopy));

  auto forBob = openOperation(file);
  ASSERT_TRUE(forBob);
  // Bob's vault holds two keys; only his signer key may sign.
  auto bobKeys = keysToSign(forBob.value().operation, h.bob.keys().value());
  ASSERT_EQ(bobKeys.size(), 1u);
  EXPECT_EQ(bobKeys[0].id, h.bobKey.id);
  EXPECT_FALSE(signWithVault(h.bob, h.carolKey.id, forBob.value().operation));
  ASSERT_TRUE(signWithVault(h.bob, h.bobKey.id, forBob.value().operation));

  auto merged = op::merge({openOperation(aliceCopy).value().operation, forBob.value().operation});
  ASSERT_TRUE(merged) << merged.error().message;
  EXPECT_TRUE(op::check(merged.value()).complete());
}

TEST(OperationsTest, AFileWithAStrangersSignatureIsRefused) {
  Holders h;
  const std::string file = h.transferFile("transfer.json");
  auto operation = op::load(file).value();
  operation.signatures.push_back(h.bob.sign(h.carolKey.id, op::signingMessage(operation)).value());
  ASSERT_TRUE(op::save(operation, h.dir / "tampered.json"));
  auto opened = openOperation(h.dir / "tampered.json");
  ASSERT_FALSE(opened);
  EXPECT_NE(opened.error().message.find("by no signer"), std::string::npos);
}

TEST(OperationsTest, SignersAreNamedFromTheAddressBookThenTheVault) {
  Holders h;
  AddressBook book;
  book.put({"Alice A.", Role::Reserve, h.aliceKey.publicKey});
  const auto bobKeys = h.bob.keys().value();
  EXPECT_EQ(signerName(h.aliceKey.publicKey, book, bobKeys).rfind("Alice A. (Reserve, ", 0), 0u);
  EXPECT_EQ(signerName(h.bobKey.publicKey, book, bobKeys).rfind("Bob (your key, ", 0), 0u);
  const std::string stranger = pp::utl::mlDsaGenerate().value().publicKey;
  EXPECT_EQ(signerName(stranger, book, bobKeys), op::fingerprint(stranger));
}

TEST(OperationsTest, AnOperationPastItsWindowHasExpired) {
  op::Summary summary;
  summary.validFrom = 100;
  summary.validUntil = 200;
  EXPECT_FALSE(expired(summary, 150));
  EXPECT_TRUE(expired(summary, 201));
  summary.validUntil = 0; // no window (a genesis)
  EXPECT_FALSE(expired(summary, 1'000'000));
}
