#pragma once
// A role app's link to the chain (DESIGN.md § Shape of the code: admin_net): one node — a relay or
// the beacon — read for what forms show and operations need, and sent complete operations.
// Calls block on the network: run them off the UI thread, one at a time.

#include "operation/Operation.h"
#include "chain/Types.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace admin::net {

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
  /** Account `id`'s latest transactions (latest first; as many as the node returns in one page). */
  pp::Roe<std::vector<pp::Ledger::Record>> transactions(uint64_t id);
  /** The least fee `record` may carry under the config in force. */
  pp::Roe<uint64_t> minimumFee(const pp::Ledger::Record &record);

  /**
   * Send a complete operation: refused when short of its quorum, or made for another network or
   * another chain of it (its genesis hash). Sending again is harmless (OPERATION_FILE.md).
   */
  pp::Roe<void> submit(const pp::operation::Operation &operation);
  /** Whether the operation's transaction is in a block (looked for among its payer's latest). */
  pp::Roe<bool> included(const pp::operation::Operation &operation);

private:
  struct State;
  std::unique_ptr<State> state_;
  std::string address_;
};

} // namespace admin::net
