#include "core/Vault.h"

#include "core/AtomicFile.h"
#include "lib/common/Utilities.h"
#include "common/io/Json.h"

#include <sodium.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace apps {

namespace {

using pp::common::Object;
using pp::common::Value;

// vault.bin: magic | version | opslimit u64 LE | memlimit u64 LE | salt | nonce | wrapped data key.
constexpr char kVaultMagic[4] = {'P', 'P', 'A', 'V'};
// A vault file: magic | version | nonce | ciphertext.
constexpr char kFileMagic[4] = {'P', 'P', 'A', 'E'};
// A backup: like vault.bin, the payload the vault's keys and files.
constexpr char kBackupMagic[4] = {'P', 'P', 'A', 'B'};
constexpr uint8_t kVersion = 1;
constexpr size_t kDataKeyBytes = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
constexpr size_t kNonceBytes = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
constexpr size_t kSaltBytes = crypto_pwhash_SALTBYTES;
constexpr size_t kKdfHeaderBytes = 4 + 1 + 8 + 8 + kSaltBytes;
constexpr const char *kVaultFile = "vault.bin";
constexpr const char *kKeysDir = "keys";
constexpr const char *kFilesDir = "files";

void wipe(std::string &secret) {
  if (!secret.empty()) {
    sodium_memzero(secret.data(), secret.size());
    secret.clear();
  }
}

/** Wipes a secret when it goes out of scope. */
struct Wiped {
  std::string value;
  ~Wiped() { wipe(value); }
};

void appendU64(std::string &out, uint64_t value) {
  for (int i = 0; i < 8; ++i) {
    out.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
  }
}

uint64_t readU64(const std::string &bytes, size_t offset) {
  uint64_t value = 0;
  for (int i = 0; i < 8; ++i) {
    value |= static_cast<uint64_t>(static_cast<uint8_t>(bytes[offset + static_cast<size_t>(i)])) << (8 * i);
  }
  return value;
}

std::string randomBytes(size_t n) {
  std::string out(n, '\0');
  randombytes_buf(out.data(), out.size());
  return out;
}

/** nonce | ciphertext of `plaintext` under `key`, bound to `aad`. */
std::string seal(const std::string &key, const std::string &plaintext, const std::string &aad) {
  const std::string nonce = randomBytes(kNonceBytes);
  std::string out = nonce;
  out.resize(kNonceBytes + plaintext.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES);
  unsigned long long length = 0;
  crypto_aead_xchacha20poly1305_ietf_encrypt(
      reinterpret_cast<unsigned char *>(out.data() + kNonceBytes), &length,
      reinterpret_cast<const unsigned char *>(plaintext.data()), plaintext.size(),
      reinterpret_cast<const unsigned char *>(aad.data()), aad.size(), nullptr,
      reinterpret_cast<const unsigned char *>(nonce.data()), reinterpret_cast<const unsigned char *>(key.data()));
  out.resize(kNonceBytes + length);
  return out;
}

pp::Roe<std::string> open(const std::string &key, const std::string &sealed, const std::string &aad) {
  if (sealed.size() < kNonceBytes + crypto_aead_xchacha20poly1305_ietf_ABYTES) {
    return pp::Error("Encrypted data is truncated");
  }
  std::string out(sealed.size() - kNonceBytes - crypto_aead_xchacha20poly1305_ietf_ABYTES, '\0');
  unsigned long long length = 0;
  if (crypto_aead_xchacha20poly1305_ietf_decrypt(
          reinterpret_cast<unsigned char *>(out.data()), &length, nullptr,
          reinterpret_cast<const unsigned char *>(sealed.data() + kNonceBytes), sealed.size() - kNonceBytes,
          reinterpret_cast<const unsigned char *>(aad.data()), aad.size(),
          reinterpret_cast<const unsigned char *>(sealed.data()), reinterpret_cast<const unsigned char *>(key.data())) !=
      0) {
    return pp::Error("Decryption failed");
  }
  out.resize(length);
  return out;
}

/** The key Argon2id derives from `passphrase` under `salt` and `limits`. */
pp::Roe<std::string> deriveKey(std::string_view passphrase, const std::string &salt, const KdfLimits &limits) {
  std::string key(kDataKeyBytes, '\0');
  if (crypto_pwhash(reinterpret_cast<unsigned char *>(key.data()), key.size(), passphrase.data(), passphrase.size(),
                    reinterpret_cast<const unsigned char *>(salt.data()), limits.opslimit,
                    static_cast<size_t>(limits.memlimit), crypto_pwhash_ALG_ARGON2ID13) != 0) {
    return pp::Error("Deriving the key from the passphrase failed (out of memory?)");
  }
  return key;
}

/** magic | version | limits | salt | sealed(payload) under a key derived from `passphrase`. */
pp::Roe<std::string> sealWithPassphrase(const char (&magic)[4], std::string_view passphrase, const KdfLimits &limits,
                                        const std::string &payload, const std::string &aad) {
  const std::string salt = randomBytes(kSaltBytes);
  auto key = deriveKey(passphrase, salt, limits);
  if (!key) {
    return key.error();
  }
  Wiped derived{std::move(key.value())};
  std::string out(magic, 4);
  out.push_back(static_cast<char>(kVersion));
  appendU64(out, limits.opslimit);
  appendU64(out, limits.memlimit);
  out += salt;
  out += seal(derived.value, payload, aad);
  return out;
}

pp::Roe<std::string> openWithPassphrase(const char (&magic)[4], std::string_view passphrase, const std::string &bytes,
                                        const std::string &aad, const std::string &what) {
  if (bytes.size() < kKdfHeaderBytes || std::memcmp(bytes.data(), magic, 4) != 0) {
    return pp::Error(what + " is not one");
  }
  if (static_cast<uint8_t>(bytes[4]) != kVersion) {
    return pp::Error(what + " has a version this app does not read");
  }
  KdfLimits limits{readU64(bytes, 5), readU64(bytes, 13)};
  if (limits.opslimit < crypto_pwhash_OPSLIMIT_MIN || limits.memlimit < crypto_pwhash_MEMLIMIT_MIN ||
      limits.memlimit > crypto_pwhash_MEMLIMIT_MAX) {
    return pp::Error(what + " is damaged");
  }
  auto key = deriveKey(passphrase, bytes.substr(21, kSaltBytes), limits);
  if (!key) {
    return key.error();
  }
  Wiped derived{std::move(key.value())};
  auto payload = open(derived.value, bytes.substr(kKdfHeaderBytes), aad);
  if (!payload) {
    return pp::Error("Wrong passphrase, or " + what + " is damaged");
  }
  return payload;
}

std::string fileAad(const std::string &kind, const std::string &name) {
  return "pp-ledger-apps/" + kind + "|" + name + "|1";
}

bool validName(const std::string &name) {
  return !name.empty() && name.size() <= 64 && name.front() != '.' &&
         std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '.'; });
}

std::string idOf(const std::string &publicKey) { return pp::utl::hexEncode(pp::utl::sha256Raw(publicKey)).substr(0, 16); }

int64_t now() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

Object keyRecord(const KeyInfo &info, const std::string &privateKey) {
  Object record;
  record.set("label", info.label);
  record.set("role", roleName(info.role));
  record.set("createdAt", static_cast<int64_t>(info.createdAt));
  record.set("publicKey", pp::utl::hexEncode(info.publicKey));
  record.set("privateKey", pp::utl::hexEncode(privateKey));
  return record;
}

pp::Roe<std::pair<KeyInfo, std::string>> fromKeyRecord(const Object &record) {
  KeyInfo info;
  info.label = record.getString("label").value_or("");
  const auto role = roleFromName(record.getString("role").value_or(""));
  info.createdAt = record.getOrDefault("createdAt", int64_t{0});
  info.publicKey = pp::utl::hexDecode(record.getString("publicKey").value_or(""));
  std::string privateKey = pp::utl::hexDecode(record.getString("privateKey").value_or(""));
  if (!role || info.publicKey.size() != pp::utl::kMlDsaPublicKeyBytes ||
      privateKey.size() != pp::utl::kMlDsaPrivateKeyBytes) {
    wipe(privateKey);
    return pp::Error("A key record is damaged");
  }
  info.role = *role;
  info.id = idOf(info.publicKey);
  return std::make_pair(std::move(info), std::move(privateKey));
}

/** `privateKey` and `publicKey` are one ML-DSA-65 pair. */
bool isPair(const std::string &privateKey, const std::string &publicKey) {
  static const std::string probe = "pp-ledger-apps/key-pair-probe/v1";
  auto signature = pp::utl::mlDsaSign(privateKey, probe);
  return signature && pp::utl::mlDsaVerify(publicKey, probe, signature.value());
}

} // namespace

KdfLimits KdfLimits::sensitive() { return {crypto_pwhash_OPSLIMIT_SENSITIVE, crypto_pwhash_MEMLIMIT_SENSITIVE}; }

KdfLimits KdfLimits::minimum() { return {crypto_pwhash_OPSLIMIT_MIN, crypto_pwhash_MEMLIMIT_MIN}; }

Vault::Vault(std::string dir, KdfLimits limits) : dir_(std::move(dir)), limits_(limits) {
  if (sodium_init() < 0) {
    std::abort(); // no randomness: nothing here could be safe
  }
}

Vault::~Vault() { lock(); }

std::string Vault::defaultDir() {
  if (const char *home = std::getenv("PP_LEDGER_APPS_HOME"); home && *home) {
    return (std::filesystem::path(home) / "vault").string();
  }
  const char *home = std::getenv("HOME");
  const std::filesystem::path base = home ? home : ".";
#ifdef __APPLE__
  return (base / "Library" / "Application Support" / "pp-ledger-apps" / "vault").string();
#else
  return (base / ".pp-ledger-apps" / "vault").string();
#endif
}

bool Vault::exists() const {
  std::error_code ec;
  return std::filesystem::exists(std::filesystem::path(dir_) / kVaultFile, ec);
}

void Vault::lock() { wipe(dataKey_); }

pp::Roe<void> Vault::requireUnlocked() const {
  if (!isUnlocked()) {
    return pp::Error("The vault is locked");
  }
  return {};
}

pp::Roe<void> Vault::writeWrapped(std::string_view passphrase, const std::string &dataKey) const {
  auto sealed = sealWithPassphrase(kVaultMagic, passphrase, limits_, dataKey, fileAad("vault", "data-key"));
  if (!sealed) {
    return sealed.error();
  }
  return writeFileAtomic((std::filesystem::path(dir_) / kVaultFile).string(), sealed.value());
}

pp::Roe<void> Vault::create(std::string_view passphrase) {
  if (exists()) {
    return pp::Error("A vault exists already in " + dir_);
  }
  if (passphrase.size() < kMinPassphraseLength) {
    return pp::Error("The passphrase needs at least " + std::to_string(kMinPassphraseLength) + " characters");
  }
  std::string dataKey = randomBytes(kDataKeyBytes);
  if (auto written = writeWrapped(passphrase, dataKey); !written) {
    wipe(dataKey);
    return written;
  }
  lock();
  dataKey_ = std::move(dataKey);
  return {};
}

pp::Roe<void> Vault::unlock(std::string_view passphrase) {
  auto bytes = apps::readFile((std::filesystem::path(dir_) / kVaultFile).string());
  if (!bytes) {
    return pp::Error("No vault in " + dir_);
  }
  auto dataKey = openWithPassphrase(kVaultMagic, passphrase, bytes.value(), fileAad("vault", "data-key"), "The vault");
  if (!dataKey) {
    return dataKey.error();
  }
  lock();
  dataKey_ = std::move(dataKey.value());
  return {};
}

pp::Roe<void> Vault::changePassphrase(std::string_view current, std::string_view next) {
  if (next.size() < kMinPassphraseLength) {
    return pp::Error("The passphrase needs at least " + std::to_string(kMinPassphraseLength) + " characters");
  }
  if (auto unlocked = unlock(current); !unlocked) {
    return unlocked;
  }
  return writeWrapped(next, dataKey_);
}

pp::Roe<void> Vault::storeKey(const KeyInfo &info, const std::string &privateKey) {
  Object record = keyRecord(info, privateKey);
  Wiped plaintext{pp::common::io::objectToJsonString(record)};
  const auto path = std::filesystem::path(dir_) / kKeysDir / (info.id + ".key");
  std::string sealed(kFileMagic, 4);
  sealed.push_back(static_cast<char>(kVersion));
  sealed += seal(dataKey_, plaintext.value, fileAad("key", info.id));
  return writeNewFile(path.string(), sealed);
}

pp::Roe<std::pair<KeyInfo, std::string>> Vault::loadKey(const std::string &id) const {
  if (auto ok = requireUnlocked(); !ok) {
    return ok.error();
  }
  if (!validName(id)) {
    return pp::Error("No key " + id + " in the vault");
  }
  auto bytes = apps::readFile((std::filesystem::path(dir_) / kKeysDir / (id + ".key")).string());
  if (!bytes) {
    return pp::Error("No key " + id + " in the vault");
  }
  if (bytes.value().size() < 5 || std::memcmp(bytes.value().data(), kFileMagic, 4) != 0 ||
      static_cast<uint8_t>(bytes.value()[4]) != kVersion) {
    return pp::Error("Key " + id + " is damaged");
  }
  auto plaintext = open(dataKey_, bytes.value().substr(5), fileAad("key", id));
  if (!plaintext) {
    return pp::Error("Key " + id + " is damaged");
  }
  Wiped json{std::move(plaintext.value())};
  Object record;
  if (!pp::common::io::objectFromJsonString(record, json.value)) {
    return pp::Error("Key " + id + " is damaged");
  }
  auto key = fromKeyRecord(record);
  if (key && key.value().first.id != id) {
    return pp::Error("Key " + id + " is damaged");
  }
  return key;
}

pp::Roe<KeyInfo> Vault::generateKey(const std::string &label, Role role) {
  if (auto ok = requireUnlocked(); !ok) {
    return ok.error();
  }
  auto pair = pp::utl::mlDsaGenerate();
  if (!pair) {
    return pp::Error("Cannot make a key: " + pair.error().message);
  }
  Wiped privateKey{pair.value().privateKey};
  wipe(pair.value().privateKey);
  KeyInfo info;
  info.label = label;
  info.role = role;
  info.publicKey = pair.value().publicKey;
  info.id = idOf(info.publicKey);
  info.createdAt = now();
  if (auto stored = storeKey(info, privateKey.value); !stored) {
    return stored.error();
  }
  return info;
}

pp::Roe<KeyInfo> Vault::importKey(const std::string &label, Role role, const std::string &privateKey,
                                  const std::string &publicKey) {
  if (auto ok = requireUnlocked(); !ok) {
    return ok.error();
  }
  if (privateKey.size() != pp::utl::kMlDsaPrivateKeyBytes || publicKey.size() != pp::utl::kMlDsaPublicKeyBytes ||
      !isPair(privateKey, publicKey)) {
    return pp::Error("Not an ML-DSA-65 key pair");
  }
  KeyInfo info;
  info.label = label;
  info.role = role;
  info.publicKey = publicKey;
  info.id = idOf(publicKey);
  info.createdAt = now();
  if (key(info.id)) {
    return pp::Error("The vault holds this key already");
  }
  if (auto stored = storeKey(info, privateKey); !stored) {
    return stored.error();
  }
  return info;
}

pp::Roe<KeyInfo> Vault::key(const std::string &id) const {
  auto loaded = loadKey(id);
  if (!loaded) {
    return loaded.error();
  }
  wipe(loaded.value().second);
  return loaded.value().first;
}

pp::Roe<std::vector<KeyInfo>> Vault::keys() const {
  if (auto ok = requireUnlocked(); !ok) {
    return ok.error();
  }
  std::vector<KeyInfo> out;
  std::error_code ec;
  const auto keysDir = std::filesystem::path(dir_) / kKeysDir;
  if (!std::filesystem::exists(keysDir, ec)) {
    return out;
  }
  for (const auto &entry : std::filesystem::directory_iterator(keysDir, ec)) {
    if (entry.path().extension() != ".key") {
      continue;
    }
    auto info = key(entry.path().stem().string());
    if (!info) {
      return info.error();
    }
    out.push_back(std::move(info.value()));
  }
  std::sort(out.begin(), out.end(), [](const KeyInfo &a, const KeyInfo &b) {
    return a.createdAt != b.createdAt ? a.createdAt < b.createdAt : a.id < b.id;
  });
  return out;
}

pp::Roe<void> Vault::withPrivateKey(const std::string &id,
                                    const std::function<pp::Roe<void>(const std::string &)> &use) const {
  auto loaded = loadKey(id);
  if (!loaded) {
    return loaded.error();
  }
  Wiped privateKey{std::move(loaded.value().second)};
  return use(privateKey.value);
}

pp::Roe<std::string> Vault::sign(const std::string &id, const std::string &message) const {
  std::string signature;
  auto used = withPrivateKey(id, [&](const std::string &privateKey) -> pp::Roe<void> {
    auto signed_ = pp::utl::mlDsaSign(privateKey, message);
    if (!signed_) {
      return signed_.error();
    }
    signature = std::move(signed_.value());
    return {};
  });
  if (!used) {
    return used.error();
  }
  return signature;
}

pp::Roe<std::string> Vault::readFile(const std::string &name) const {
  if (auto ok = requireUnlocked(); !ok) {
    return ok.error();
  }
  if (!validName(name)) {
    return pp::Error("Not a vault file name: " + name);
  }
  const auto path = std::filesystem::path(dir_) / kFilesDir / name;
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return std::string();
  }
  auto bytes = apps::readFile(path.string());
  if (!bytes) {
    return bytes.error();
  }
  if (bytes.value().size() < 5 || std::memcmp(bytes.value().data(), kFileMagic, 4) != 0 ||
      static_cast<uint8_t>(bytes.value()[4]) != kVersion) {
    return pp::Error("Vault file " + name + " is damaged");
  }
  auto plaintext = open(dataKey_, bytes.value().substr(5), fileAad("file", name));
  if (!plaintext) {
    return pp::Error("Vault file " + name + " is damaged");
  }
  return plaintext;
}

pp::Roe<void> Vault::writeFile(const std::string &name, const std::string &plaintext) {
  if (auto ok = requireUnlocked(); !ok) {
    return ok;
  }
  if (!validName(name)) {
    return pp::Error("Not a vault file name: " + name);
  }
  std::string sealed(kFileMagic, 4);
  sealed.push_back(static_cast<char>(kVersion));
  sealed += seal(dataKey_, plaintext, fileAad("file", name));
  return writeFileAtomic((std::filesystem::path(dir_) / kFilesDir / name).string(), sealed);
}

pp::Roe<void> Vault::exportBackup(const std::string &path, std::string_view backupPassphrase) const {
  if (backupPassphrase.size() < kMinPassphraseLength) {
    return pp::Error("The backup passphrase needs at least " + std::to_string(kMinPassphraseLength) + " characters");
  }
  auto infos = keys();
  if (!infos) {
    return infos.error();
  }
  std::vector<Value> keyRecords;
  for (const auto &info : infos.value()) {
    auto loaded = loadKey(info.id);
    if (!loaded) {
      return loaded.error();
    }
    Wiped privateKey{std::move(loaded.value().second)};
    keyRecords.emplace_back(std::make_shared<Object>(keyRecord(info, privateKey.value)));
  }
  Object files;
  std::error_code ec;
  const auto filesDir = std::filesystem::path(dir_) / kFilesDir;
  if (std::filesystem::exists(filesDir, ec)) {
    for (const auto &entry : std::filesystem::directory_iterator(filesDir, ec)) {
      const std::string name = entry.path().filename().string();
      if (!validName(name)) {
        continue;
      }
      auto content = readFile(name);
      if (!content) {
        return content.error();
      }
      files.set(name, pp::utl::hexEncode(content.value()));
    }
  }
  Object backup;
  backup.set("keys", Object::array(std::move(keyRecords)));
  backup.set("files", files);
  Wiped payload{pp::common::io::objectToJsonString(backup)};
  auto sealed = sealWithPassphrase(kBackupMagic, backupPassphrase, limits_, payload.value, fileAad("backup", "all"));
  if (!sealed) {
    return sealed.error();
  }
  return writeNewFile(path, sealed.value());
}

pp::Roe<std::vector<KeyInfo>> Vault::importBackup(const std::string &path, std::string_view backupPassphrase) {
  if (auto ok = requireUnlocked(); !ok) {
    return ok.error();
  }
  auto bytes = apps::readFile(path);
  if (!bytes) {
    return bytes.error();
  }
  auto payload = openWithPassphrase(kBackupMagic, backupPassphrase, bytes.value(), fileAad("backup", "all"),
                                    "The backup");
  if (!payload) {
    return payload.error();
  }
  Wiped json{std::move(payload.value())};
  Object backup;
  if (!pp::common::io::objectFromJsonString(backup, json.value)) {
    return pp::Error("The backup is damaged");
  }
  std::vector<KeyInfo> added;
  if (const auto *records = backup.getArray("keys")) {
    for (const auto &value : records->elements) {
      const Object *record = pp::common::asObject(value);
      if (!record) {
        return pp::Error("The backup is damaged");
      }
      auto key = fromKeyRecord(*record);
      if (!key) {
        return key.error();
      }
      Wiped privateKey{std::move(key.value().second)};
      if (this->key(key.value().first.id)) {
        continue;
      }
      if (auto stored = storeKey(key.value().first, privateKey.value); !stored) {
        return stored.error();
      }
      added.push_back(key.value().first);
    }
  }
  if (const Object *files = backup.getObject("files")) {
    for (const auto &[name, value] : files->entries()) {
      const auto hex = pp::common::asString(value);
      if (!validName(name) || !hex) {
        return pp::Error("The backup is damaged");
      }
      auto current = readFile(name);
      if (current && current.value().empty()) {
        if (auto written = writeFile(name, pp::utl::hexDecode(*hex)); !written) {
          return written.error();
        }
      }
    }
  }
  return added;
}

} // namespace apps
