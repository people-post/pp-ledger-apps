#pragma once
// A vault key to and from a key backup (pp-ledger docs/contracts/KEY_BACKUP.md): the one-key file wallets
// exchange — how a person brings their key from a wallet plugin, and takes it elsewhere (WL005).

#include "core/Vault.h"
#include "ledger/operation/KeyBackup.h"

#include <string>
#include <string_view>

namespace apps {

/** Key `id` of `vault` as a key backup under `passphrase` (its label inside, sealed). */
pp::Roe<std::string> exportKeyBackup(const Vault &vault, const std::string &id, std::string_view passphrase,
                                     const pp::operation::KeyBackupLimits &limits =
                                         pp::operation::KeyBackupLimits::sensitive());
/** A key backup's key, decrypted with `passphrase`, kept in `vault` for `role`. */
pp::Roe<KeyInfo> importKeyBackup(Vault &vault, const std::string &json, std::string_view passphrase, Role role);

} // namespace apps
