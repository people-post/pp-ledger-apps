#include "net/WalletChain.h"

#include "net/TokenOperations.h"
#include "ledger/chain/AccountBuffer.h"
#include "ledger/chain/ClientVerify.h"
#include "ledger/chain/Domains.h"
#include "ledger/chain/RecordHandler.h"
#include "ledger/client/AccountIds.h"
#include "ledger/lib/common/BinaryPack.hpp"

#include <algorithm>
#include <cctype>
#include <string>

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
    out.attachmentHash = proven.value().attachmentHash;
    out.provenAtBlock = proven.value().blockId;
    out.tip = checked.value().first;
    return out;
  }
  return pp::Error(why);
}

pp::Roe<VerifiedDomain> Verifier::domain(ChainLink &link, const std::string &typed) {
  auto name = pp::chain_domain::normalize(typed);
  if (!name) {
    return pp::Error("Not a domain: 1 to 7 letters A-Z");
  }
  std::string why;
  for (int attempt = 0; attempt < 3; ++attempt) {
    auto proof = link.domainProof(*name);
    if (!proof) {
      why = proof.error().message;
      continue;
    }
    auto checked = tipAndBlocks(link, proof.value().blockId);
    if (!checked) {
      why = checked.error().message;
      continue;
    }
    auto proven = pp::verify::checkDomainProof(checked.value().second.back().block, proof.value());
    if (!proven) {
      return pp::Error("The node's domain proof does not hold: " + proven.error().message);
    }
    VerifiedDomain out;
    out.domain = *name;
    out.accountId = proven.value().accountId;
    out.provenAtBlock = proven.value().blockId;
    out.tip = checked.value().first;
    return out;
  }
  return pp::Error(why);
}

pp::Roe<uint64_t> Verifier::accountOf(ChainLink &link, const std::string &typed) {
  std::string text;
  for (char c : typed) {
    if (c != ' ' && c != ',' && c != '\t') {
      text.push_back(c);
    }
  }
  if (!text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); })) {
    try {
      return static_cast<uint64_t>(std::stoull(text));
    } catch (const std::exception &) {
      return pp::Error("Not an account number: " + typed);
    }
  }
  auto found = domain(link, text);
  if (!found) {
    return found.error();
  }
  if (!found.value().accountId) {
    return pp::Error("No account has the domain " + found.value().domain + " (proven)");
  }
  return *found.value().accountId;
}

pp::operation::SignerSet signersOf(const VerifiedAccount &account) {
  pp::operation::SignerSet signers;
  signers.account = account.id;
  signers.publicKeys = account.wallet.publicKeys;
  signers.minSignatures = account.wallet.minSignatures;
  return signers;
}

pp::Roe<pp::UserAccount> provenState(ChainLink &link, const VerifiedAccount &account) {
  if (!account.exists) {
    return pp::Error("Account " + std::to_string(account.id) + " does not exist");
  }
  auto state = link.account(account.id);
  if (!state) {
    return state.error();
  }
  if (pp::AccountBuffer::attachmentHashOf(state.value().meta) != account.attachmentHash) {
    return pp::Error("The node's copy of the account's attachment is not the proven one");
  }
  pp::UserAccount out;
  out.wallet = account.wallet;
  out.meta = state.value().meta;
  return out;
}

pp::Roe<pp::operation::Operation> makeMoveAll(const ChainInfo &chain, const pp::operation::SignerSet &from,
                                              const pp::UserAccount &state, uint64_t to, const Making &making) {
  if (!from.account || *from.account == to) {
    return pp::Error("Move it to another account");
  }
  if (auto ok = checkNotFrozen(state); !ok) {
    return ok.error();
  }
  pp::Ledger::TxDefault probe;
  probe.fromWalletId = *from.account;
  auto least = pp::minimumFeeFor(chain.meta.config, pp::Ledger::TypedTx(probe));
  if (!least) {
    return pp::Error("Cannot work out the fee: " + least.error().message);
  }
  const uint64_t fee = least.value();
  std::vector<pp::operation::Payment> payments;
  for (const auto &[token, amount] : state.wallet.mBalances) {
    if (token != pp::AccountIds::ID_GENESIS && token != *from.account && amount > 0) {
      payments.push_back({to, static_cast<uint64_t>(amount), token});
    }
  }
  const auto native = state.wallet.mBalances.find(pp::AccountIds::ID_GENESIS);
  const uint64_t held = native == state.wallet.mBalances.end() || native->second < 0 ? 0 : static_cast<uint64_t>(native->second);
  const uint64_t fees = fee * (payments.size() + 1);
  if (held <= fees) {
    return pp::Error("The account holds " + pp::operation::groupDigits(held) + " of the native token: not enough for " +
                     std::to_string(payments.size() + 1) + " transfers' fees (" + pp::operation::groupDigits(fees) + ")");
  }
  payments.push_back({to, held - fees, 0});
  pp::operation::Common common;
  common.networkId = chain.networkId;
  common.genesisHash = chain.genesisHash;
  common.signers = from;
  common.window = pp::operation::windowFrom(making.now, making.validForSeconds);
  common.notes = making.notes;
  common.createdAt = making.now;
  if (payments.size() == 1) {
    return pp::operation::transfer(common, to, payments.front().amount, fee);
  }
  return pp::operation::transfers(common, payments, fee);
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
