#pragma once
// The wallet's view of the chain (projects/wallet-app/DESIGN.md, WL003): every read verified from the
// network card's genesis hash — block 0, the authority proof to the beacon key, the beacon's signed
// tip, the blocks under it, an account's Merkle path (pp-ledger's verify::). A node can refuse to
// answer, or answer with an older tip; it cannot make the wallet show what the chain does not hold.

#include "net/ChainLink.h"
#include "core/WalletBook.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace apps::net {

/** A tip the beacon signed, checked back to block 0. */
struct VerifiedTip {
  uint64_t height{0};
  uint64_t generation{0};
  uint64_t epoch{0};
  /** The tip block's timestamp (Unix seconds). */
  int64_t timestamp{0};
};

/** An account proven in a block under a verified tip. */
struct VerifiedAccount {
  uint64_t id{0};
  bool exists{false};
  pp::Client::Wallet wallet;
  uint64_t provenAtBlock{0};
  VerifiedTip tip;
};

/**
 * Verified reads through one node, from `card`'s anchor. Keeps block 0 once checked; everything else
 * is checked afresh each read.
 */
class Verifier {
public:
  explicit Verifier(wallet::NetworkCard card) : card_(std::move(card)) {}

  /** The node's signed tip, checked: the authority proof from block 0, the beacon's signature, the block. */
  pp::Roe<VerifiedTip> tip(ChainLink &link);
  /** Account `id` (or its absence) proven under a verified tip. */
  pp::Roe<VerifiedAccount> account(ChainLink &link, uint64_t id);

  const wallet::NetworkCard &card() const { return card_; }

private:
  /** Block 0, checked against the card (its hash, its network) once. */
  pp::Roe<pp::Ledger::ChainNode> genesis(ChainLink &link);
  /** The signed tip checked, and the blocks from it down to `downTo` (the tip's alone by default). */
  pp::Roe<std::pair<VerifiedTip, std::vector<pp::Ledger::ChainNode>>> tipAndBlocks(ChainLink &link,
                                                                                 std::optional<uint64_t> downTo);

  wallet::NetworkCard card_;
  std::optional<pp::Ledger::ChainNode> genesis_;
};

/** What one node of the card says: its verified tip, or why not. */
struct NodeView {
  std::string address;
  std::optional<VerifiedTip> tip;
  std::string error;
};

/** Each of `card`'s nodes asked for its tip, verified, one link each (blocking: off the UI thread). */
std::vector<NodeView> checkNodes(const wallet::NetworkCard &card);

/** Blocks a node may trail the highest verified tip before the wallet says it is behind. */
inline constexpr uint64_t kBehindBlocks = 3;

} // namespace apps::net
