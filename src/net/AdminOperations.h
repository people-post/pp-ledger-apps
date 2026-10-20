#pragma once
// The admin set's operations (DESIGN.md § Who uses them: the Admin app), built against the chain as
// a node shows it (ChainInfo) and checked with pp-ledger's own rules before anyone signs: a mint to
// the reserve, a config update (settings and/or the admin set itself), the next beacon certificate.

#include "net/ChainLink.h"
#include "ledger/chain/Genesis.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace apps::net {

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
                                                    const Making &making, const std::string &domain = "");
/** The least fee of a new account (of `keys`, with `domain` if any) under the config in force. */
pp::Roe<uint64_t> newAccountFee(const ChainInfo &chain, const std::vector<std::string> &keys, uint32_t minSignatures,
                                const std::string &domain = "");
/**
 * Issued account `id` (as `state` holds it: it has no domain yet) given its domain (pp-ledger
 * projects/domains: 4–7 capital letters), signed by the registrar, which pays the least fee. Whether
 * another account has it is the chain's to refuse.
 */
pp::Roe<pp::operation::Operation> makeDomainAssignment(const ChainInfo &chain,
                                                       const pp::operation::SignerSet &registrar, uint64_t id,
                                                       const pp::UserAccount &state, const std::string &domain,
                                                       const Making &making);

/**
 * A recovery announcement (T_RECOVERY): the recovery set in force names a new admin set, applied its
 * delay later (beacon-authority ba7). Signed by the recovery set — no account.
 */
pp::Roe<pp::operation::Operation> makeRecovery(const ChainInfo &chain, const std::vector<std::string> &adminKeys,
                                               uint32_t minSignatures, const Making &making);

/** The settings a genesis plan sets: an update's, and the slot timing (fixed from then on). */
const std::vector<ConfigField> &genesisFields();

/** Who holds a system account or key set in a genesis plan. */
struct PlanKeys {
  std::vector<std::string> publicKeys;
  uint32_t minSignatures{0};
};

/** A genesis plan as a form fills it (DESIGN.md § The genesis ceremony). */
struct GenesisForm {
  pp::BlockChainConfig config; ///< networkId and genesisTime (the start) included
  PlanKeys admin, reserve, registrar, fee, recycle, recovery;
  /** The system accounts' domains (1–3 letters; the genesis account's is the native token's symbol). */
  pp::chain_genesis::SystemNames names = pp::chain_genesis::defaultSystemNames();
  uint64_t recoveryDelayEpochs{0};
  std::string beaconKey;
  /** Genesis miners: their key and stake (ids assigned from 2^20 up). */
  std::vector<std::pair<std::string, uint64_t>> miners;
};

/** Block 0 as an operation for the admin set to sign offline (pp-beacon --init --genesis-op). */
pp::Roe<pp::operation::Operation> makeGenesisPlan(const GenesisForm &form, const std::string &notes, int64_t now);

} // namespace apps::net
