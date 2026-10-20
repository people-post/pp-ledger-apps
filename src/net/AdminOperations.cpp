#include "net/AdminOperations.h"

#include "ledger/chain/AccountPolicy.h"
#include "ledger/chain/ConfigPolicy.h"
#include "ledger/chain/Domains.h"
#include "ledger/chain/Genesis.h"
#include "ledger/chain/RecordHandler.h"
#include "ledger/client/AccountAttachment.h"
#include "ledger/client/AccountIds.h"
#include "ledger/lib/common/Crypto.h"
#include "ledger/lib/common/BinaryPack.hpp"

#include <sodium.h>

#include <algorithm>

namespace apps::net {

namespace op = pp::operation;

namespace {

ConfigField field(std::string name, std::string title, uint64_t pp::BlockChainConfig::*member) {
  return {std::move(name), std::move(title), [member](const pp::BlockChainConfig &c) { return c.*member; },
          [member](pp::BlockChainConfig &c, uint64_t v) { c.*member = v; }};
}

ConfigField feeCoefficient(size_t index, std::string title) {
  return {"minFeeCoefficients." + std::to_string(index), std::move(title),
          [index](const pp::BlockChainConfig &c) -> uint64_t {
            return index < c.minFeeCoefficients.size() ? c.minFeeCoefficients[index] : 0;
          },
          [index](pp::BlockChainConfig &c, uint64_t v) {
            if (c.minFeeCoefficients.size() <= index) {
              c.minFeeCoefficients.resize(index + 1, 0);
            }
            c.minFeeCoefficients[index] = static_cast<uint16_t>(v);
          }};
}

/** The admin set in force signs, as account 0. */
op::Common adminCommon(const ChainInfo &chain, const Making &making) {
  op::Common common;
  common.networkId = chain.networkId;
  common.genesisHash = chain.genesisHash;
  common.signers.account = pp::AccountIds::ID_GENESIS;
  common.signers.publicKeys = chain.meta.genesis.wallet.publicKeys;
  common.signers.minSignatures = chain.meta.genesis.wallet.minSignatures;
  common.window = op::windowFrom(making.now, making.validForSeconds);
  common.notes = making.notes;
  common.createdAt = making.now;
  return common;
}

pp::Roe<void> checkValidity(const ChainInfo &chain, const Making &making) {
  const uint64_t longest = chain.meta.config.maxValidationTimespanSeconds;
  if (making.validForSeconds <= 0 || (longest > 0 && static_cast<uint64_t>(making.validForSeconds) > longest)) {
    return pp::Error("The chain lets an operation wait at most " + std::to_string(longest / 3600) + " hours");
  }
  return {};
}

uint64_t randomId() {
  uint64_t id = 0;
  while (id == 0) {
    randombytes_buf(&id, sizeof(id));
  }
  return id;
}

template <typename Tx> void stamp(Tx &tx, const op::Common &common) {
  tx.idempotentId = randomId();
  tx.validationTsMin = common.window.from;
  tx.validationTsMax = common.window.until;
}

std::string grouped(uint64_t value) { return op::groupDigits(value); }

} // namespace

const std::vector<ConfigField> &configFields() {
  using C = pp::BlockChainConfig;
  static const std::vector<ConfigField> fields = {
      field("maxTransactionsPerBlock", "Transactions per block, most", &C::maxTransactionsPerBlock),
      field("maxBlockBytes", "Block bytes, most", &C::maxBlockBytes),
      field("maxValidationTimespanSeconds", "Transaction validity, longest (seconds)",
            &C::maxValidationTimespanSeconds),
      field("maxIssuancePerEpoch", "Mint per epoch, most", &C::maxIssuancePerEpoch),
      field("newAccountFee", "New account fee", &C::newAccountFee),
      feeCoefficient(0, "Fee: base"),
      feeCoefficient(1, "Fee: per KiB of metadata"),
      feeCoefficient(2, "Fee: per KiB² of metadata"),
      {"freeCustomMetaSize", "Free metadata bytes",
       [](const C &c) -> uint64_t { return c.freeCustomMetaSize; },
       [](C &c, uint64_t v) { c.freeCustomMetaSize = static_cast<uint32_t>(v); }},
      field("maxCustomMetaSize", "Metadata bytes, most", &C::maxCustomMetaSize),
      field("heartbeatSlots", "Heartbeat slots", &C::heartbeatSlots),
      field("minerMinStake", "Miner stake, least", &C::minerMinStake),
      field("minerRegistrationFee", "Miner registration fee", &C::minerRegistrationFee),
      field("minerDueAfterSlots", "Miner due after (slots)", &C::minerDueAfterSlots),
      field("minerMaxMissedSlots", "Miner missed slots, most", &C::minerMaxMissedSlots),
      field("minerReregisterCooldownEpochs", "Miner re-registration wait (epochs)", &C::minerReregisterCooldownEpochs),
      {"checkpoint.minBlocks", "Checkpoint, least blocks",
       [](const C &c) -> uint64_t { return c.checkpoint.minBlocks; },
       [](C &c, uint64_t v) { c.checkpoint.minBlocks = v; }},
      {"checkpoint.minAgeSeconds", "Checkpoint, least age (seconds)",
       [](const C &c) -> uint64_t { return c.checkpoint.minAgeSeconds; },
       [](C &c, uint64_t v) { c.checkpoint.minAgeSeconds = v; }},
  };
  return fields;
}

std::vector<ConfigChange> configChanges(const pp::BlockChainConfig &before, const pp::BlockChainConfig &after) {
  std::vector<ConfigChange> changes;
  for (const auto &f : configFields()) {
    if (f.get(before) != f.get(after)) {
      changes.push_back({f.title, grouped(f.get(before)), grouped(f.get(after))});
    }
  }
  return changes;
}

pp::Roe<op::Operation> makeMint(const ChainInfo &chain, uint64_t amount, uint64_t fee, const Making &making) {
  if (auto ok = checkValidity(chain, making); !ok) {
    return ok.error();
  }
  // The cap counts amount and fee; 0 mints nothing until a config update raises it.
  const uint64_t cap = chain.meta.config.maxIssuancePerEpoch;
  if (amount == 0) {
    return pp::Error("Mint an amount above 0");
  }
  if (cap == 0) {
    return pp::Error("The chain mints nothing until a config update raises its mint cap (Config)");
  }
  if (amount > cap || fee > cap - amount) {
    return pp::Error("The chain mints at most " + grouped(cap) + " per epoch, fee included");
  }
  auto mint = op::mint(adminCommon(chain, making), amount, fee);
  if (!mint) {
    return mint.error();
  }
  auto typed = mint.value().record.decode();
  auto least = typed ? pp::minimumFeeFor(chain.meta.config, typed.value()) : pp::chain_tx::Roe<uint64_t>(0);
  if (least && fee < least.value()) {
    return pp::Error("The fee must be at least " + grouped(least.value()));
  }
  return mint;
}

pp::Roe<op::Operation> makeConfigUpdate(const ChainInfo &chain, const pp::BlockChainConfig &config,
                                        const std::optional<pp::Wallet> &admins, const Making &making) {
  if (auto ok = checkValidity(chain, making); !ok) {
    return ok.error();
  }
  const auto &now = chain.meta.config;
  if (chain.meta.hasPending) {
    return pp::Error("A config update is pending already (from epoch " + grouped(chain.meta.pending.activationEpoch) +
                     "): one at a time");
  }
  if (auto frozen = pp::chain_tx::requireAdminSetNotFrozen(chain.meta.authority, "A config update"); !frozen) {
    return pp::Error(frozen.error().message);
  }
  if (config.networkId != now.networkId || config.genesisTime != now.genesisTime ||
      config.slotDuration != now.slotDuration || config.slotsPerEpoch != now.slotsPerEpoch) {
    return pp::Error("The network, genesis time and slot timing are fixed at genesis");
  }
  if (auto liveness = pp::chain_tx::validateLivenessSettings(config); !liveness) {
    return pp::Error(liveness.error().message);
  }
  if (auto limits = pp::chain_tx::validateBlockLimits(config); !limits) {
    return pp::Error(limits.error().message);
  }
  pp::GenesisAccountMeta next = chain.meta;
  next.config = config;
  next.hasPending = false;
  next.pending = {};
  if (admins) {
    next.genesis.wallet.publicKeys = admins->publicKeys;
    next.genesis.wallet.minSignatures = admins->minSignatures;
    if (auto shape = pp::chain_tx::validateGenesisWalletShape(next.genesis.wallet); !shape) {
      return pp::Error("The admin set: " + shape.error().message);
    }
  }
  const bool adminsChange = next.genesis.wallet.publicKeys != chain.meta.genesis.wallet.publicKeys ||
                            next.genesis.wallet.minSignatures != chain.meta.genesis.wallet.minSignatures;
  if (configChanges(now, config).empty() && !adminsChange) {
    return pp::Error("Nothing changes");
  }
  const op::Common common = adminCommon(chain, making);
  pp::Ledger::TxConfig tx;
  tx.fee = 0;
  tx.meta = next.ltsToString();
  stamp(tx, common);
  pp::Ledger::Record record;
  record.type = pp::Ledger::T_CONFIG;
  record.data = pp::utl::binaryPack(tx);
  return op::wrap(common, record);
}

pp::Roe<op::Operation> makeBeaconCertificate(const ChainInfo &chain, const std::string &publicKey,
                                             const std::vector<std::string> &dialHints, const Making &making) {
  if (auto ok = checkValidity(chain, making); !ok) {
    return ok.error();
  }
  const auto &authority = chain.meta.authority;
  if (auto frozen = pp::chain_tx::requireAdminSetNotFrozen(authority, "A beacon certificate"); !frozen) {
    return pp::Error(frozen.error().message);
  }
  pp::BeaconCertificate cert;
  cert.networkId = chain.networkId;
  cert.generation = authority.beacon.generation + 1;
  cert.publicKey = publicKey;
  cert.cutoverHeight = pp::BeaconCertificate::kNoCutover;
  cert.dialHints = dialHints;
  if (auto hints = pp::chain_tx::validateCertificateHints(cert); !hints) {
    return pp::Error(hints.error().message);
  }
  if (auto valid = pp::chain_tx::validateNextBeaconCertificate(cert, authority, chain.meta.genesis.wallet,
                                                                chain.networkId, std::nullopt);
      !valid) {
    return pp::Error(valid.error().message);
  }
  const op::Common common = adminCommon(chain, making);
  pp::Ledger::TxBeaconCert tx;
  tx.fee = 0;
  tx.meta = cert.ltsToString();
  stamp(tx, common);
  pp::Ledger::Record record;
  record.type = pp::Ledger::T_BEACON_CERT;
  record.data = pp::utl::binaryPack(tx);
  return op::wrap(common, record);
}

bool isIssuedId(uint64_t id) {
  return id >= pp::AccountIds::ID_FIRST_ISSUED && id < pp::AccountIds::ID_FIRST_USER;
}

namespace {

pp::UserAccount ownerAccount(const std::vector<std::string> &keys, uint32_t minSignatures, uint64_t amount,
                             const std::string &domain) {
  pp::UserAccount account;
  account.wallet.domain = domain;
  account.wallet.publicKeys = keys;
  account.wallet.minSignatures = static_cast<uint8_t>(minSignatures);
  account.wallet.keyType = pp::Crypto::TK_ML_DSA_65;
  account.wallet.mBalances[pp::AccountIds::ID_GENESIS] = static_cast<int64_t>(amount);
  account.meta = pp::AccountAttachment::emptySerialized();
  return account;
}

} // namespace

pp::Roe<uint64_t> newAccountFee(const ChainInfo &chain, const std::vector<std::string> &keys, uint32_t minSignatures,
                                const std::string &domain) {
  pp::Ledger::TxNewUser probe;
  probe.fromWalletId = pp::AccountIds::ID_REGISTRAR;
  probe.meta = ownerAccount(keys, minSignatures, 0, domain).ltsToString();
  // The metadata fee and the config's flat newAccountFee (pp-ledger's rule).
  auto fee = pp::minimumFeeFor(chain.meta.config, pp::Ledger::TypedTx(probe));
  if (!fee) {
    return pp::Error(fee.error().message);
  }
  return fee.value();
}

pp::Roe<op::Operation> makeIssuedAccount(const ChainInfo &chain, const op::SignerSet &registrar, uint64_t id,
                                         const std::vector<std::string> &keys, uint32_t minSignatures,
                                         uint64_t amount, uint64_t fee, const Making &making,
                                         const std::string &domain) {
  if (auto ok = checkValidity(chain, making); !ok) {
    return ok.error();
  }
  if (!isIssuedId(id)) {
    return pp::Error("Issued account numbers run from " + grouped(pp::AccountIds::ID_FIRST_ISSUED) + " to " +
                     grouped(pp::AccountIds::ID_FIRST_USER - 1));
  }
  if (keys.empty()) {
    return pp::Error("Add the owner's key");
  }
  for (const auto &k : keys) {
    if (k.size() != pp::utl::kMlDsaPublicKeyBytes) {
      return pp::Error("An owner key is not an ML-DSA-65 public key");
    }
    if (std::count(keys.begin(), keys.end(), k) > 1) {
      return pp::Error("The same key is listed twice");
    }
  }
  if (minSignatures == 0 || minSignatures > keys.size() || minSignatures > 255) {
    return pp::Error("Signatures needed: between 1 and the owner's keys");
  }
  if (registrar.account != pp::AccountIds::ID_REGISTRAR) {
    return pp::Error("Issued accounts are created by the registrar");
  }
  if (!domain.empty()) {
    if (auto valid = pp::chain_domain::validateFor(domain, id); !valid) {
      return pp::Error(valid.error().message);
    }
  }
  auto least = newAccountFee(chain, keys, minSignatures, domain);
  if (!least) {
    return least.error();
  }
  if (fee < least.value()) {
    return pp::Error("The fee must be at least " + grouped(least.value()));
  }
  op::Common common;
  common.networkId = chain.networkId;
  common.genesisHash = chain.genesisHash;
  common.signers = registrar;
  common.window = op::windowFrom(making.now, making.validForSeconds);
  common.notes = making.notes;
  common.createdAt = making.now;
  return op::newAccount(common, id, ownerAccount(keys, minSignatures, amount, domain), amount, fee);
}

pp::Roe<op::Operation> makeDomainAssignment(const ChainInfo &chain, const op::SignerSet &registrar, uint64_t id,
                                            const pp::UserAccount &state, const std::string &domain,
                                            const Making &making) {
  if (auto ok = checkValidity(chain, making); !ok) {
    return ok.error();
  }
  if (registrar.account != pp::AccountIds::ID_REGISTRAR) {
    return pp::Error("The registrar gives domains");
  }
  if (!isIssuedId(id)) {
    return pp::Error("Only issued accounts get a domain from the registrar");
  }
  if (!state.wallet.domain.empty()) {
    return pp::Error("Account " + grouped(id) + " has its domain already: " + state.wallet.domain);
  }
  if (auto valid = pp::chain_domain::validateFor(domain, id); !valid) {
    return pp::Error(valid.error().message);
  }
  op::Common common;
  common.networkId = chain.networkId;
  common.genesisHash = chain.genesisHash;
  common.signers = registrar;
  common.window = op::windowFrom(making.now, making.validForSeconds);
  common.notes = making.notes;
  common.createdAt = making.now;
  auto probe = op::assignDomain(common, id, domain, 0);
  if (!probe) {
    return probe;
  }
  auto typed = probe.value().record.decode();
  if (!typed) {
    return pp::Error("The assignment does not decode: " + typed.error().message);
  }
  auto least = pp::minimumFeeFor(chain.meta.config, typed.value());
  if (!least) {
    return pp::Error("Cannot work out the fee: " + least.error().message);
  }
  return op::assignDomain(common, id, domain, least.value());
}

pp::Roe<op::Operation> makeRecovery(const ChainInfo &chain, const std::vector<std::string> &adminKeys,
                                    uint32_t minSignatures, const Making &making) {
  if (auto ok = checkValidity(chain, making); !ok) {
    return ok.error();
  }
  if (minSignatures > 255) {
    return pp::Error("At most 255 signatures");
  }
  pp::RecoveryAnnouncement announcement;
  announcement.networkId = chain.networkId;
  announcement.adminPublicKeys = adminKeys;
  announcement.adminMinSignatures = static_cast<uint8_t>(minSignatures);
  const auto &authority = chain.meta.authority;
  if (auto valid = pp::chain_tx::validateRecoveryAnnouncement(announcement, authority, chain.networkId); !valid) {
    return pp::Error(valid.error().message);
  }
  op::Common common;
  common.networkId = chain.networkId;
  common.genesisHash = chain.genesisHash;
  common.signers.publicKeys = authority.recovery.publicKeys; // the recovery set signs: no account
  common.signers.minSignatures = authority.recovery.minSignatures;
  common.window = op::windowFrom(making.now, making.validForSeconds);
  common.notes = making.notes;
  common.createdAt = making.now;
  pp::Ledger::TxRecovery tx;
  tx.fee = 0;
  tx.meta = announcement.ltsToString();
  stamp(tx, common);
  pp::Ledger::Record record;
  record.type = pp::Ledger::T_RECOVERY;
  record.data = pp::utl::binaryPack(tx);
  return op::wrap(common, record);
}

const std::vector<ConfigField> &genesisFields() {
  using C = pp::BlockChainConfig;
  static const std::vector<ConfigField> fields = [] {
    std::vector<ConfigField> all = {field("slotDuration", "Slot (seconds)", &C::slotDuration),
                                    field("slotsPerEpoch", "Slots per epoch", &C::slotsPerEpoch)};
    for (const auto &f : configFields()) {
      all.push_back(f);
    }
    return all;
  }();
  return fields;
}

pp::Roe<op::Operation> makeGenesisPlan(const GenesisForm &form, const std::string &notes, int64_t now) {
  if (form.config.networkId.empty()) {
    return pp::Error("Name the network");
  }
  if (form.config.genesisTime <= 0) {
    return pp::Error("Set when the network starts");
  }
  if (form.beaconKey.size() != pp::utl::kMlDsaPublicKeyBytes) {
    return pp::Error("Choose the beacon's key (its operator's key card)");
  }
  if (auto liveness = pp::chain_tx::validateLivenessSettings(form.config); !liveness) {
    return pp::Error(liveness.error().message);
  }
  if (auto limits = pp::chain_tx::validateBlockLimits(form.config); !limits) {
    return pp::Error(limits.error().message);
  }
  auto keys = [](const PlanKeys &k) {
    return pp::chain_genesis::AccountKeys{k.publicKeys, static_cast<uint8_t>(std::min<uint32_t>(k.minSignatures, 255))};
  };
  pp::chain_genesis::Plan plan;
  plan.config = form.config;
  plan.keys.genesis = keys(form.admin);
  plan.keys.reserve = keys(form.reserve);
  plan.keys.registrar = keys(form.registrar);
  plan.keys.fee = keys(form.fee);
  plan.keys.recycle = keys(form.recycle);
  plan.names = form.names;
  for (const auto *k : {&form.admin, &form.reserve, &form.registrar, &form.fee, &form.recycle, &form.recovery}) {
    if (k->publicKeys.empty() || k->minSignatures == 0 || k->minSignatures > k->publicKeys.size()) {
      return pp::Error("Every account and the recovery set need keys, and between 1 and that many to sign");
    }
  }
  uint64_t id = pp::AccountIds::ID_FIRST_ISSUED;
  for (const auto &[publicKey, stake] : form.miners) {
    plan.miners.push_back({id++, {publicKey}, 1, stake});
  }
  plan.authority.recovery.publicKeys = form.recovery.publicKeys;
  plan.authority.recovery.minSignatures = static_cast<uint8_t>(std::min<uint32_t>(form.recovery.minSignatures, 255));
  plan.authority.recovery.delayEpochs = form.recoveryDelayEpochs;
  plan.authority.beacon.generation = 1;
  plan.authority.beacon.publicKey = form.beaconKey;
  plan.authority.beacon.cutoverHeight = 0;
  auto records = pp::chain_genesis::records(plan);
  if (!records) {
    return pp::Error(records.error().message);
  }
  return op::genesis(records.value(), notes, now);
}

} // namespace apps::net
