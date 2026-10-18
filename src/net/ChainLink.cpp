#include "net/ChainLink.h"

#include "chain/RecordHandler.h"
#include "client/Client.h"
#include "lib/common/BinaryPack.hpp"
#include "lib/common/Utilities.h"
#include "network/LedgerAmpRuntime.h"
#include "network/amp/AmpIdentity.h"

namespace admin::net {

namespace op = pp::operation;

struct ChainLink::State {
  pp::network::LedgerAmpRuntime runtime;
  pp::Client client;
  bool started{false};
  ~State() {
    if (started) {
      runtime.Stop();
    }
  }
};

ChainLink::ChainLink() = default;

ChainLink::~ChainLink() = default;

bool ChainLink::connected() const { return state_ != nullptr; }

pp::Roe<void> ChainLink::connect(const std::string &multiaddr) {
  state_.reset();
  address_.clear();
  if (multiaddr.empty() || multiaddr.front() != '/') {
    return pp::Error("Not a node address: it starts with /ip4/ or /dns4/ and ends with /p2p/…");
  }
  // An identity of its own each time: nodes need not know the app.
  auto identity = pp::utl::mlDsaGenerate();
  if (!identity) {
    return pp::Error("Cannot make a network identity: " + identity.error().message);
  }
  auto config = pp::network::LedgerAmpConfigFromPrivateKey(identity.value().privateKey, 0);
  if (!config) {
    return pp::Error("Cannot set up the network: " + config.error().message);
  }
  auto state = std::make_unique<State>();
  if (auto started = state->runtime.Start(config.value()); !started) {
    return pp::Error("Cannot start the network: " + started.error().message);
  }
  state->started = true;
  state->client.attachAmpTransport(state->runtime, "node");
  if (auto dialed = state->client.setAmpPeer("node", multiaddr); !dialed) {
    return pp::Error("Cannot reach " + multiaddr + ": " + dialed.error().message);
  }
  state_ = std::move(state);
  address_ = multiaddr;
  return {};
}

pp::Roe<ChainInfo> ChainLink::info() {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  ChainInfo info;
  auto networkId = state_->client.fetchNetworkId();
  if (!networkId) {
    return pp::Error("The node does not answer: " + networkId.error().message);
  }
  info.networkId = networkId.value();
  auto genesis = state_->client.fetchBlock(0);
  if (!genesis) {
    return pp::Error("Cannot read block 0: " + genesis.error().message);
  }
  info.genesisHash = genesis.value().hash;
  auto meta = state_->client.fetchGenesisMeta();
  if (!meta) {
    return pp::Error("Cannot read the config: " + meta.error().message);
  }
  info.meta = std::move(meta.value());
  if (auto status = state_->client.fetchBeaconState()) {
    info.nextBlockId = status.value().nextBlockId;
  }
  return info;
}

pp::Roe<pp::UserAccount> ChainLink::account(uint64_t id) {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  auto account = state_->client.fetchUserAccount(id);
  if (!account) {
    return pp::Error("Cannot read account " + std::to_string(id) + ": " + account.error().message);
  }
  return account.value();
}

pp::Roe<op::SignerSet> ChainLink::signersOf(uint64_t id) {
  auto holder = account(id);
  if (!holder) {
    return holder.error();
  }
  op::SignerSet signers;
  signers.account = id;
  signers.publicKeys = holder.value().wallet.publicKeys;
  signers.minSignatures = holder.value().wallet.minSignatures;
  return signers;
}

pp::Roe<std::vector<pp::Ledger::Record>> ChainLink::transactions(uint64_t id) {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  pp::Client::TxGetByWalletRequest request;
  request.walletId = id;
  auto found = state_->client.fetchTransactionsByWallet(request);
  if (!found) {
    return pp::Error("Cannot read the account's transactions: " + found.error().message);
  }
  return std::move(found.value().transactions);
}

pp::Roe<uint64_t> ChainLink::minimumFee(const pp::Ledger::Record &record) {
  auto chain = info();
  if (!chain) {
    return chain.error();
  }
  auto typed = record.decode();
  if (!typed) {
    return pp::Error("The transaction does not decode: " + typed.error().message);
  }
  auto fee = pp::minimumFeeFor(chain.value().meta.config, typed.value());
  if (!fee) {
    return pp::Error("Cannot work out the fee: " + fee.error().message);
  }
  return fee.value();
}

pp::Roe<void> ChainLink::submit(const op::Operation &operation) {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  if (operation.kind == op::Kind::Genesis) {
    return pp::Error("A genesis is not submitted: it starts the beacon");
  }
  auto record = op::signedRecord(operation);
  if (!record) {
    return record.error();
  }
  auto chain = info();
  if (!chain) {
    return chain.error();
  }
  if (chain.value().networkId != operation.networkId) {
    return pp::Error("This operation is for network " + operation.networkId + ", the node serves " +
                     chain.value().networkId);
  }
  if (!operation.genesisHash.empty() && chain.value().genesisHash != operation.genesisHash) {
    return pp::Error("This operation is for another chain of network " + operation.networkId);
  }
  auto sent = state_->client.submitTransaction(record.value());
  if (!sent) {
    return pp::Error("The node refused it: " + sent.error().message);
  }
  return {};
}

pp::Roe<bool> ChainLink::included(const op::Operation &operation) {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  if (operation.kind == op::Kind::Recovery) {
    // No account pays it: in a block once the chain holds its announcement, pending or applied.
    auto tx = pp::utl::binaryUnpack<pp::Ledger::TxRecovery>(operation.record.data);
    pp::RecoveryAnnouncement announcement;
    if (!tx || !announcement.ltsFromString(tx.value().meta)) {
      return pp::Error("The recovery does not decode");
    }
    auto chain = info();
    if (!chain) {
      return chain.error();
    }
    const auto &meta = chain.value().meta;
    const auto &pending = meta.authority.pendingRecovery.announcement;
    const bool isPending = meta.authority.hasPendingRecovery &&
                           pending.adminPublicKeys == announcement.adminPublicKeys &&
                           pending.adminMinSignatures == announcement.adminMinSignatures;
    const bool applied = meta.genesis.wallet.publicKeys == announcement.adminPublicKeys &&
                         meta.genesis.wallet.minSignatures == announcement.adminMinSignatures;
    return isPending || applied;
  }
  const auto payer = op::signerAccountOf(operation.record);
  if (!payer) {
    return pp::Error("This kind of operation is not followed into a block");
  }
  auto found = transactions(*payer);
  if (!found) {
    return found.error();
  }
  for (const auto &record : found.value()) {
    if (record.type == operation.record.type && record.data == operation.record.data) {
      return true;
    }
  }
  return false;
}

} // namespace admin::net
