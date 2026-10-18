#include "net/AdminOperations.h"

#include "chain/AccountPolicy.h"
#include "chain/ConfigPolicy.h"
#include "chain/RecordHandler.h"
#include "client/AccountIds.h"
#include "lib/common/BinaryPack.hpp"

#include <sodium.h>

namespace admin::net {

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

} // namespace admin::net
