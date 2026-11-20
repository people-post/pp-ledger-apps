#include "core/KeyBackups.h"

#include <sodium.h>

namespace apps {

pp::Roe<std::string> exportKeyBackup(const Vault &vault, const std::string &id, std::string_view passphrase,
                                     const pp::operation::KeyBackupLimits &limits) {
  auto info = vault.key(id);
  if (!info) {
    return info.error();
  }
  std::string json;
  auto made = vault.withPrivateKey(id, [&](const std::string &privateKey) -> pp::Roe<void> {
    pp::operation::BackedUpKey key{info.value().label, info.value().publicKey, privateKey};
    auto written = pp::operation::keyBackupToJson(key, passphrase, limits);
    sodium_memzero(key.privateKey.data(), key.privateKey.size());
    if (!written) {
      return written.error();
    }
    json = std::move(written.value());
    return {};
  });
  if (!made) {
    return made.error();
  }
  return json;
}

pp::Roe<KeyInfo> importKeyBackup(Vault &vault, const std::string &json, std::string_view passphrase, Role role) {
  auto key = pp::operation::keyBackupFromJson(json, passphrase);
  if (!key) {
    return key.error();
  }
  auto imported = vault.importKey(key.value().label.empty() ? "imported key" : key.value().label, role,
                                  key.value().privateKey, key.value().publicKey);
  sodium_memzero(key.value().privateKey.data(), key.value().privateKey.size());
  return imported;
}

} // namespace apps
