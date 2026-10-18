// A vault key to a key backup and into another vault: what lets a person move their key between wallets.
#include "core/KeyBackups.h"
#include "TempDir.h"

#include "lib/common/Utilities.h"

#include <gtest/gtest.h>

using namespace apps;

TEST(KeyBackupsTest, AKeyMovesBetweenVaultsThroughABackup) {
  test::TempDir dir;
  Vault from(dir / "from", KdfLimits::minimum());
  Vault to(dir / "to", KdfLimits::minimum());
  ASSERT_TRUE(from.create("first vault passphrase"));
  ASSERT_TRUE(to.create("second vault passphrase"));
  auto key = from.generateKey("savings", Role::Owner);
  ASSERT_TRUE(key);

  EXPECT_FALSE(exportKeyBackup(from, key.value().id, "short", pp::operation::KeyBackupLimits::minimum()));
  auto json = exportKeyBackup(from, key.value().id, "a backup passphrase", pp::operation::KeyBackupLimits::minimum());
  ASSERT_TRUE(json) << json.error().message;
  EXPECT_EQ(pp::operation::keyBackupPublicKey(json.value()).value(), key.value().publicKey);

  EXPECT_FALSE(importKeyBackup(to, json.value(), "not the passphrase", Role::Owner));
  auto imported = importKeyBackup(to, json.value(), "a backup passphrase", Role::Owner);
  ASSERT_TRUE(imported) << imported.error().message;
  EXPECT_EQ(imported.value().publicKey, key.value().publicKey);
  EXPECT_EQ(imported.value().label, "savings");
  // The same key: what one vault signs, the other's key verifies — and the other signs too.
  auto signature = to.sign(imported.value().id, "message");
  ASSERT_TRUE(signature);
  EXPECT_TRUE(pp::utl::mlDsaVerify(key.value().publicKey, "message", signature.value()));
}
