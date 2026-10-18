#pragma once
// The admin set's operations (DESIGN.md § Who uses them: the Admin app), built against the chain as
// a node shows it (ChainInfo) and checked with pp-ledger's own rules before anyone signs: a mint to
// the reserve, a config update (settings and/or the admin set itself), the next beacon certificate.

#include "net/ChainLink.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace admin::net {

/** A setting of the chain config an update may change. */
struct ConfigField {
  std::string name;  ///< "maxBlockBytes"
  std::string title; ///< "Block bytes, most"
  std::function<uint64_t(const pp::BlockChainConfig &)> get;
  std::function<void(pp::BlockChainConfig &, uint64_t)> set;
};

/** The settings an update may change (slot timing and the network are fixed at genesis). */
const std::vector<ConfigField> &configFields();

/** One setting changed: "Block bytes, most", "4,194,304" → "2,097,152". */
struct ConfigChange {
  std::string title;
  std::string before;
  std::string after;
};
std::vector<ConfigChange> configChanges(const pp::BlockChainConfig &before, const pp::BlockChainConfig &after);

/** The validity, notes and time an operation is made with. */
struct Making {
  int64_t now{0};
  int64_t validForSeconds{86400};
  std::string notes;
};

/**
 * New native tokens to the reserve, within the chain's issuance cap per epoch (amount and fee; a
 * cap of 0 mints nothing). Mints in one epoch add up: the chain refuses what goes past the cap.
 */
pp::Roe<pp::operation::Operation> makeMint(const ChainInfo &chain, uint64_t amount, uint64_t fee, const Making &making);

/**
 * A config update (T_CONFIG): `config` as the next settings, `admins` (none: the same) as the next
 * admin set. Refused while another update is pending or the admin set is frozen (a recovery
 * pending), when nothing changes, or when the chain would refuse it.
 */
pp::Roe<pp::operation::Operation> makeConfigUpdate(const ChainInfo &chain, const pp::BlockChainConfig &config,
                                                   const std::optional<pp::Wallet> &admins, const Making &making);

/** The next beacon generation's certificate (T_BEACON_CERT) for `publicKey`, no cutover. */
pp::Roe<pp::operation::Operation> makeBeaconCertificate(const ChainInfo &chain, const std::string &publicKey,
                                                        const std::vector<std::string> &dialHints,
                                                        const Making &making);

/** The issued account ids the registrar creates: [2^20, 2^30). */
bool isIssuedId(uint64_t id);

/**
 * A new issued account `id` (T_NEW_USER from the registrar, signed by `registrar`'s keys): its
 * owner's `keys`, `minSignatures` of them to sign, `amount` to start with. The fee is at least the
 * metadata fee plus the config's newAccountFee.
 */
pp::Roe<pp::operation::Operation> makeIssuedAccount(const ChainInfo &chain, const pp::operation::SignerSet &registrar,
                                                    uint64_t id, const std::vector<std::string> &keys,
                                                    uint32_t minSignatures, uint64_t amount, uint64_t fee,
                                                    const Making &making);
/** The least fee of a new account (of `keys`) under the config in force. */
pp::Roe<uint64_t> newAccountFee(const ChainInfo &chain, const std::vector<std::string> &keys, uint32_t minSignatures);

} // namespace admin::net
