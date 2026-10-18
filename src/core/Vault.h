#pragma once
// The vault: one per person per machine, shared by the apps (DESIGN.md § Vault, AD006). A passphrase
// unwraps a random data key (Argon2id); every file in the vault — each private key, the address
// book — is encrypted under it (XChaCha20-Poly1305), bound to its name. Private keys never leave
// the vault but encrypted, in a backup.

#include "core/Roles.h"
#include "common/Error.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace admin {

/** What deriving the key from the passphrase costs (Argon2id). Stored with the vault. */
struct KdfLimits {
  uint64_t opslimit{0};
  uint64_t memlimit{0};
  /** libsodium's sensitive limits: unlocking is rare, and the keys are worth it. */
  static KdfLimits sensitive();
  /** The cheapest libsodium allows: tests only. */
  static KdfLimits minimum();
};

/** A key in the vault, without its private key. */
struct KeyInfo {
  /** 16 hex digits of the public key's SHA-256 (its fingerprint, without dashes). */
  std::string id;
  std::string label;
  Role role{Role::Admin};
  /** Raw ML-DSA-65 public key. */
  std::string publicKey;
  int64_t createdAt{0};
};

class Vault {
public:
  /** The vault in `dir`; a new vault or passphrase change derives with `limits`. */
  explicit Vault(std::string dir, KdfLimits limits = KdfLimits::sensitive());
  ~Vault();
  Vault(const Vault &) = delete;
  Vault &operator=(const Vault &) = delete;

  /**
   * This machine's vault directory: $PP_LEDGER_ADMIN_HOME/vault, else ~/.pp-ledger-admin/vault
   * (Linux) or ~/Library/Application Support/pp-ledger-admin/vault (macOS).
   */
  static std::string defaultDir();
  /** Shortest passphrase a vault takes. */
  static constexpr size_t kMinPassphraseLength = 10;

  const std::string &dir() const { return dir_; }
  bool exists() const;
  bool isUnlocked() const { return !dataKey_.empty(); }

  /** A new, empty vault, unlocked. Refuses when one exists. */
  pp::Roe<void> create(std::string_view passphrase);
  pp::Roe<void> unlock(std::string_view passphrase);
  /** Forget the data key (wiped). */
  void lock();
  /** Re-wrap the data key under `next`; the files are untouched. */
  pp::Roe<void> changePassphrase(std::string_view current, std::string_view next);

  /** A new ML-DSA-65 key pair, kept in the vault. */
  pp::Roe<KeyInfo> generateKey(const std::string &label, Role role);
  /** A key made elsewhere (pp-client keygen -o: its .key and .pub); the pair is checked. */
  pp::Roe<KeyInfo> importKey(const std::string &label, Role role, const std::string &privateKey,
                             const std::string &publicKey);
  /** The keys held, oldest first. */
  pp::Roe<std::vector<KeyInfo>> keys() const;
  pp::Roe<KeyInfo> key(const std::string &id) const;
  /** Sign `message` with key `id`. */
  pp::Roe<std::string> sign(const std::string &id, const std::string &message) const;
  /** Run `use` on key `id`'s private key; the copy is wiped after. */
  pp::Roe<void> withPrivateKey(const std::string &id,
                               const std::function<pp::Roe<void>(const std::string &privateKey)> &use) const;

  /** A file of the vault's own (`name`: letters, digits, '-', '.'), decrypted; "" when absent. */
  pp::Roe<std::string> readFile(const std::string &name) const;
  pp::Roe<void> writeFile(const std::string &name, const std::string &plaintext);

  /** Every key and vault file, encrypted under `backupPassphrase`, to a new file `path`. */
  pp::Roe<void> exportBackup(const std::string &path, std::string_view backupPassphrase) const;
  /** Add a backup's keys and files to this vault (keys held already are kept); the keys added. */
  pp::Roe<std::vector<KeyInfo>> importBackup(const std::string &path, std::string_view backupPassphrase);

private:
  pp::Roe<void> requireUnlocked() const;
  pp::Roe<void> storeKey(const KeyInfo &info, const std::string &privateKey);
  pp::Roe<std::pair<KeyInfo, std::string>> loadKey(const std::string &id) const;
  pp::Roe<void> writeWrapped(std::string_view passphrase, const std::string &dataKey) const;

  std::string dir_;
  KdfLimits limits_;
  std::string dataKey_;
};

} // namespace admin
