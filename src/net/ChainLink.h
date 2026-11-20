#pragma once
// A role app's link to the chain (DESIGN.md § Shape of the code: apps_net): one node — a relay or
// the beacon — read for what forms show and operations need, and sent complete operations.
// Calls block on the network: run them off the UI thread, one at a time.

#include "ledger/operation/Operation.h"
#include "ledger/chain/Types.h"
#include "ledger/client/Client.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace apps::net {

/** What a node says of its chain. */
struct ChainInfo {
  std::string networkId;
  /** Raw 32-byte hash of block 0: binds operations to this chain. */
  std::string genesisHash;
  uint64_t nextBlockId{0};
  /** The config in force (and any update pending), the admin keys, the authority. */
  pp::GenesisAccountMeta meta;
};

class ChainLink {
public:
  ChainLink();
  ~ChainLink();
  ChainLink(const ChainLink &) = delete;
  ChainLink &operator=(const ChainLink &) = delete;

  /** Link to the node at `multiaddr` (an ADP multiaddr, as its operator gives it). */
  pp::Roe<void> connect(const std::string &multiaddr);
  bool connected() const;
  const std::string &address() const { return address_; }

  pp::Roe<ChainInfo> info();
  pp::Roe<pp::UserAccount> account(uint64_t id);
  /** Account `id`'s keys and threshold: who signs for it. */
  pp::Roe<pp::operation::SignerSet> signersOf(uint64_t id);
  /** Block `id` (its header and records). */
  pp::Roe<pp::Ledger::ChainNode> block(uint64_t id);
  /** The beacon's latest signed tip as the node holds it (STATUS); none when it holds none. */
  pp::Roe<std::optional<pp::Client::SignedTip>> signedTip();
  /** AUTHORITY_PROOF up to block `upTo`: the records that changed the beacon authority. */
  pp::Roe<pp::Client::AuthorityProof> authorityProof(uint64_t upTo);
  /** ACCOUNT_PROOF: account `id` in the node's tip state, with its Merkle path. */
  pp::Roe<pp::Client::AccountProof> accountProof(uint64_t id);
  /** DOMAIN_GET: `domain` (capital letters) in the node's tip state, with its Merkle path. */
  pp::Roe<pp::Client::DomainProof> domainProof(const std::string &domain);
  /** One page of PRODUCER_STATS for blocks [from, to) (the node caps a page). */
  pp::Roe<pp::Client::ProducerStatsResponse> producerStats(uint64_t from, uint64_t to);
  /** Account `id`'s latest transactions (latest first; as many as the node returns in one page). */
  pp::Roe<std::vector<pp::Ledger::Record>> transactions(uint64_t id);
  /** The least fee `record` may carry under the config in force. */
  pp::Roe<uint64_t> minimumFee(const pp::Ledger::Record &record);

  /**
   * Send a complete operation: refused when short of its quorum, or made for another network or
   * another chain of it (its genesis hash). Sending again is harmless (OPERATION_FILE.md). A batch
   * goes transfer by transfer; refused when any is (the others are sent).
   */
  pp::Roe<void> submit(const pp::operation::Operation &operation);
  /** Whether the operation's transactions are all in blocks (looked for among their payer's latest). */
  pp::Roe<bool> included(const pp::operation::Operation &operation);

private:
  /** Pages of the payer's transactions looked through for an operation's (about 32 blocks each). */
  static constexpr int kIncludedPages = 16;
  struct State;
  std::unique_ptr<State> state_;
  std::string address_;
};

} // namespace apps::net
