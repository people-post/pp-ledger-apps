// The vault: a passphrase unlocks it, keys stay in it, a backup restores them elsewhere.
#include "core/Vault.h"
#include "TempDir.h"

#include "lib/common/Utilities.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sys/stat.h>

using namespace apps;

namespace {

constexpr const char *kPassphrase = "correct horse battery";

std::string fileBytes(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

TEST(VaultTest, APassphraseUnlocksItAndNoOtherDoes) {
  test::TempDir dir;
  {
    Vault vault(dir / "vault", KdfLimits::minimum());
    EXPECT_FALSE(vault.exists());
    EXPECT_FALSE(vault.create("short"));
    ASSERT_TRUE(vault.create(kPassphrase));
    EXPECT_TRUE(vault.isUnlocked());
    EXPECT_FALSE(vault.create(kPassphrase)); // one vault per directory
  }
  Vault vault(dir / "vault", KdfLimits::minimum());
  EXPECT_TRUE(vault.exists());
  EXPECT_FALSE(vault.isUnlocked());
  EXPECT_FALSE(vault.keys());
  EXPECT_FALSE(vault.unlock("not the passphrase"));
  EXPECT_FALSE(vault.isUnlocked());
  ASSERT_TRUE(vault.unlock(kPassphrase));
  EXPECT_TRUE(vault.isUnlocked());
  vault.lock();
  EXPECT_FALSE(vault.isUnlocked());
}

TEST(VaultTest, KeysStayInTheVaultEncryptedAndSign) {
  test::TempDir dir;
  Vault vault(dir / "vault", KdfLimits::minimum());
  ASSERT_TRUE(vault.create(kPassphrase));
  auto key = vault.generateKey("Alice reserve", Role::Reserve);
  ASSERT_TRUE(key) << key.error().message;
  EXPECT_EQ(key.value().id.size(), 16u);
  EXPECT_EQ(key.value().publicKey.size(), pp::utl::kMlDsaPublicKeyBytes);

  auto signature = vault.sign(key.value().id, "message");
  ASSERT_TRUE(signature);
  EXPECT_TRUE(pp::utl::mlDsaVerify(key.value().publicKey, "message", signature.value()));

  // On disk: owner-only, and neither the label nor the private key in clear.
  const std::string path = dir / ("vault/keys/" + key.value().id + ".key");
  struct stat st {};
  ASSERT_EQ(::stat(path.c_str(), &st), 0);
  EXPECT_EQ(st.st_mode & 0777, 0600);
  const std::string bytes = fileBytes(path);
  EXPECT_EQ(bytes.find("Alice"), std::string::npos);
  EXPECT_EQ(bytes.find("privateKey"), std::string::npos);

  // Another session lists it after unlocking.
  Vault again(dir / "vault", KdfLimits::minimum());
  ASSERT_TRUE(again.unlock(kPassphrase));
  auto keys = again.keys();
  ASSERT_TRUE(keys);
  ASSERT_EQ(keys.value().size(), 1u);
  EXPECT_EQ(keys.value()[0].label, "Alice reserve");
  EXPECT_EQ(keys.value()[0].role, Role::Reserve);
  EXPECT_EQ(keys.value()[0].publicKey, key.value().publicKey);
}

TEST(VaultTest, AKeyFileMovedToAnotherNameDoesNotOpen) {
  test::TempDir dir;
  Vault vault(dir / "vault", KdfLimits::minimum());
  ASSERT_TRUE(vault.create(kPassphrase));
  auto a = vault.generateKey("a", Role::Admin).value();
  auto b = vault.generateKey("b", Role::Admin).value();
  std::filesystem::copy_file(dir / ("vault/keys/" + a.id + ".key"), dir / ("vault/keys/" + b.id + ".key"),
                             std::filesystem::copy_options::overwrite_existing);
  EXPECT_TRUE(vault.key(a.id));
  EXPECT_FALSE(vault.key(b.id));
  EXPECT_FALSE(vault.sign(b.id, "m"));
}

TEST(VaultTest, ImportTakesOnlyAKeyPair) {
  test::TempDir dir;
  Vault vault(dir / "vault", KdfLimits::minimum());
  ASSERT_TRUE(vault.create(kPassphrase));
  auto pair = pp::utl::mlDsaGenerate().value();
  auto other = pp::utl::mlDsaGenerate().value();
  EXPECT_FALSE(vault.importKey("mismatch", Role::Admin, pair.privateKey, other.publicKey));
  auto imported = vault.importKey("keygen", Role::Admin, pair.privateKey, pair.publicKey);
  ASSERT_TRUE(imported) << imported.error().message;
  EXPECT_FALSE(vault.importKey("again", Role::Admin, pair.privateKey, pair.publicKey));
  auto signature = vault.sign(imported.value().id, "m");
  ASSERT_TRUE(signature);
  EXPECT_TRUE(pp::utl::mlDsaVerify(pair.publicKey, "m", signature.value()));
}

TEST(VaultTest, ChangingThePassphraseKeepsTheKeys) {
  test::TempDir dir;
  Vault vault(dir / "vault", KdfLimits::minimum());
  ASSERT_TRUE(vault.create(kPassphrase));
  auto key = vault.generateKey("k", Role::Registrar).value();
  EXPECT_FALSE(vault.changePassphrase("wrong passphrase", "a new passphrase"));
  ASSERT_TRUE(vault.changePassphrase(kPassphrase, "a new passphrase"));
  Vault again(dir / "vault", KdfLimits::minimum());
  EXPECT_FALSE(again.unlock(kPassphrase));
  ASSERT_TRUE(again.unlock("a new passphrase"));
  EXPECT_TRUE(again.key(key.id));
}

TEST(VaultTest, ABackupRestoresKeysAndFilesOnAnotherMachine) {
  test::TempDir dir;
  Vault vault(dir / "vault", KdfLimits::minimum());
  ASSERT_TRUE(vault.create(kPassphrase));
  auto key = vault.generateKey("Admin 1", Role::Admin).value();
  ASSERT_TRUE(vault.writeFile("address-book.json", "{\"cards\": []}"));
  EXPECT_FALSE(vault.exportBackup(dir / "backup.bin", "short"));
  ASSERT_TRUE(vault.exportBackup(dir / "backup.bin", "the backup passphrase"));
  EXPECT_FALSE(vault.exportBackup(dir / "backup.bin", "the backup passphrase")); // never overwritten
  EXPECT_EQ(fileBytes(dir / "backup.bin").find("Admin 1"), std::string::npos);

  Vault restored(dir / "elsewhere", KdfLimits::minimum());
  ASSERT_TRUE(restored.create("another passphrase"));
  EXPECT_FALSE(restored.importBackup(dir / "backup.bin", "not the backup passphrase"));
  auto added = restored.importBackup(dir / "backup.bin", "the backup passphrase");
  ASSERT_TRUE(added) << added.error().message;
  ASSERT_EQ(added.value().size(), 1u);
  EXPECT_EQ(added.value()[0].label, "Admin 1");
  auto signature = restored.sign(key.id, "m");
  ASSERT_TRUE(signature);
  EXPECT_TRUE(pp::utl::mlDsaVerify(key.publicKey, "m", signature.value()));
  EXPECT_EQ(restored.readFile("address-book.json").value(), "{\"cards\": []}");
  // Again: nothing new.
  EXPECT_TRUE(restored.importBackup(dir / "backup.bin", "the backup passphrase").value().empty());
}

TEST(VaultTest, VaultFilesAreEncryptedAndNamedSafely) {
  test::TempDir dir;
  Vault vault(dir / "vault", KdfLimits::minimum());
  ASSERT_TRUE(vault.create(kPassphrase));
  EXPECT_EQ(vault.readFile("settings.json").value(), "");
  ASSERT_TRUE(vault.writeFile("settings.json", "secret settings"));
  EXPECT_EQ(vault.readFile("settings.json").value(), "secret settings");
  EXPECT_EQ(fileBytes(dir / "vault/files/settings.json").find("secret"), std::string::npos);
  EXPECT_FALSE(vault.writeFile("../escape", "x"));
  EXPECT_FALSE(vault.readFile(".hidden"));
}
