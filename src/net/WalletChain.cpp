#include "net/WalletChain.h"

#include "chain/ClientVerify.h"
#include "lib/common/BinaryPack.hpp"

namespace apps::net {

namespace {

/** Blocks the wallet reads down from a signed tip to a proof's block, at most. */
constexpr uint64_t kMaxBlocksUnderTip = 64;

} // namespace

pp::Roe<pp::Ledger::ChainNode> Verifier::genesis(ChainLink &link) {
  if (genesis_) {
    return *genesis_;
  }
  auto block = link.block(0);
  if (!block) {
    return block.error();
  }
  if (auto ok = pp::verify::checkGenesis(block.value(), card_.genesisHash); !ok) {
    return pp::Error(ok.error().message + ": this node serves another chain");
  }
  // Its network: the genesis record's config.
  const auto &records = block.value().block.records;
  auto tx = records.empty() ? pp::Roe<pp::Ledger::TxGenesis>(pp::Error("no genesis record"))
                            : pp::utl::binaryUnpack<pp::Ledger::TxGenesis>(records.front().data);
  pp::GenesisAccountMeta meta;
  if (!tx || !meta.ltsFromString(tx.value().meta) || meta.config.networkId != card_.networkId) {
    return pp::Error("Block 0 is not network " + card_.networkId + "'s");
  }
  genesis_ = block.value();
  return *genesis_;
}

pp::Roe<std::pair<VerifiedTip, std::vector<pp::Ledger::ChainNode>>>
Verifier::tipAndBlocks(ChainLink &link, std::optional<uint64_t> downTo) {
  auto zero = genesis(link);
  if (!zero) {
    return zero.error();
  }
  auto signedTip = link.signedTip();
  if (!signedTip) {
    return signedTip.error();
  }
  if (!signedTip.value()) {
    return pp::Error("The node holds no tip the beacon signed");
  }
  const pp::Client::SignedTip tip = *signedTip.value();
  const uint64_t bottom = downTo.value_or(tip.height);
  if (bottom > tip.height || tip.height - bottom > kMaxBlocksUnderTip) {
    return pp::Error("The node's state is not near its signed tip: ask again");
  }
  std::vector<pp::Ledger::ChainNode> blocks;
  for (uint64_t id = tip.height + 1; id-- > bottom;) {
    auto block = link.block(id);
    if (!block) {
      return block.error();
    }
    blocks.push_back(std::move(block.value()));
  }
  auto proof = link.authorityProof(tip.height);
  if (!proof) {
    return proof.error();
  }
  auto trail = pp::verify::authorityAt(zero.value(), card_.genesisHash, proof.value(), tip.height,
                                       blocks.front().block.epoch);
  if (!trail) {
    return pp::Error("The beacon authority does not follow from block 0: " + trail.error().message);
  }
  if (auto ok = trail.value().verifyTip(tip); !ok) {
    return pp::Error("The tip is not the beacon's: " + ok.error().message);
  }
  if (auto ok = pp::verify::checkBlocksUnderTip(tip, blocks); !ok) {
    return pp::Error(ok.error().message);
  }
  VerifiedTip verified;
  verified.height = tip.height;
  verified.generation = tip.generation;
  verified.epoch = blocks.front().block.epoch;
  verified.timestamp = blocks.front().block.timestamp;
  return std::make_pair(verified, std::move(blocks));
}

pp::Roe<VerifiedTip> Verifier::tip(ChainLink &link) {
  auto checked = tipAndBlocks(link, std::nullopt);
  if (!checked) {
    return checked.error();
  }
  return checked.value().first;
}

pp::Roe<VerifiedAccount> Verifier::account(ChainLink &link, uint64_t id) {
  std::string why;
  // The node's state may be a block ahead of the beacon's latest signature: a moment later it is not.
  for (int attempt = 0; attempt < 3; ++attempt) {
    auto proof = link.accountProof(id);
    if (!proof) {
      why = proof.error().message;
      continue;
    }
    auto checked = tipAndBlocks(link, proof.value().blockId);
    if (!checked) {
      why = checked.error().message;
      continue;
    }
    auto proven = pp::verify::checkAccountProof(checked.value().second.back().block, proof.value());
    if (!proven) {
      return pp::Error("The node's account proof does not hold: " + proven.error().message);
    }
    VerifiedAccount out;
    out.id = id;
    out.exists = proven.value().wallet.has_value();
    if (out.exists) {
      out.wallet = *proven.value().wallet;
    }
    out.provenAtBlock = proven.value().blockId;
    out.tip = checked.value().first;
    return out;
  }
  return pp::Error(why);
}

std::vector<NodeView> checkNodes(const wallet::NetworkCard &card) {
  std::vector<NodeView> views;
  for (const auto &address : card.nodes) {
    NodeView view;
    view.address = address;
    ChainLink link;
    if (auto linked = link.connect(address); !linked) {
      view.error = linked.error().message;
    } else {
      Verifier verifier(card);
      auto tip = verifier.tip(link);
      if (tip) {
        view.tip = tip.value();
      } else {
        view.error = tip.error().message;
      }
    }
    views.push_back(std::move(view));
  }
  return views;
}

} // namespace apps::net
