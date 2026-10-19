#include "net/ChainLink.h"

#include "ledger/chain/RecordHandler.h"
#include "ledger/client/Client.h"
#include "ledger/lib/common/BinaryPack.hpp"
#include "ledger/lib/common/Utilities.h"
#include "ledger/network/LedgerAmpRuntime.h"
#include "ledger/network/amp/AmpIdentity.h"

#include <algorithm>

namespace apps::net {

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

pp::Roe<pp::Ledger::ChainNode> ChainLink::block(uint64_t id) {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  auto found = state_->client.fetchBlock(id);
  if (!found) {
    return pp::Error("Cannot read block " + std::to_string(id) + ": " + found.error().message);
  }
  return found.value();
}

pp::Roe<std::optional<pp::Client::SignedTip>> ChainLink::signedTip() {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  auto status = state_->client.fetchBeaconState();
  if (!status) {
    return pp::Error("The node does not answer: " + status.error().message);
  }
  return status.value().signedTip;
}

pp::Roe<pp::Client::AuthorityProof> ChainLink::authorityProof(uint64_t upTo) {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  auto proof = state_->client.fetchAuthorityProof(upTo);
  if (!proof) {
    return pp::Error("Cannot read the authority proof: " + proof.error().message);
  }
  return proof.value();
}

pp::Roe<pp::Client::AccountProof> ChainLink::accountProof(uint64_t id) {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  auto proof = state_->client.fetchAccountProof(id);
  if (!proof) {
    return pp::Error("Cannot read the proof of account " + std::to_string(id) + ": " + proof.error().message);
  }
  return proof.value();
}

pp::Roe<pp::Client::DomainProof> ChainLink::domainProof(const std::string &domain) {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  auto proof = state_->client.fetchDomainProof(domain);
  if (!proof) {
    return pp::Error("Cannot read the proof of domain " + domain + ": " + proof.error().message);
  }
  return proof.value();
}

pp::Roe<pp::Client::ProducerStatsResponse> ChainLink::producerStats(uint64_t from, uint64_t to) {
  if (!state_) {
    return pp::Error("Not linked to a node");
  }
  auto stats = state_->client.fetchProducerStats({from, to});
  if (!stats) {
    return pp::Error("Cannot count the blocks' producers: " + stats.error().message);
  }
  return stats.value();
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
  auto records = op::signedRecords(operation);
  if (!records) {
    return records.error();
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
  size_t refused = 0;
  std::string why;
  for (const auto &record : records.value()) {
    if (auto sent = state_->client.submitTransaction(record); !sent) {
      ++refused;
      why = sent.error().message;
    }
  }
  if (refused == records.value().size() && refused == 1) {
    return pp::Error("The node refused it: " + why);
  }
  if (refused > 0) {
    return pp::Error("The node refused " + std::to_string(refused) + " of its " +
                     std::to_string(records.value().size()) + " transfers (the others were sent): " + why);
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
  std::vector<const pp::Ledger::Record *> wanted{&operation.record};
  for (const auto &more : operation.moreRecords) {
    wanted.push_back(&more);
  }
  // The payer's latest transactions, page by page back (a page is a few dozen blocks): a batch's
  // transfers may spread over several.
  pp::Client::TxGetByWalletRequest request;
  request.walletId = *payer;
  for (int page = 0; page < kIncludedPages && !wanted.empty(); ++page) {
    auto found = state_->client.fetchTransactionsByWallet(request);
    if (!found) {
      return pp::Error("Cannot read the account's transactions: " + found.error().message);
    }
    std::erase_if(wanted, [&found](const pp::Ledger::Record *record) {
      return std::any_of(found.value().transactions.begin(), found.value().transactions.end(),
                         [record](const pp::Ledger::Record &r) { return r.type == record->type && r.data == record->data; });
    });
    if (found.value().nextBlockId == 0 || found.value().nextBlockId == request.beforeBlockId) {
      break; // the start of the chain
    }
    request.beforeBlockId = found.value().nextBlockId;
  }
  return wanted.empty();
}

} // namespace apps::net
