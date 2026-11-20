#include "net/TokenOperations.h"

#include "ledger/chain/AccountPolicy.h"
#include "ledger/chain/RecordHandler.h"
#include "ledger/client/AccountIds.h"

#include <algorithm>
#include <limits>

namespace apps::net {

namespace op = pp::operation;

namespace {

std::string grouped(uint64_t value) { return op::groupDigits(value); }

std::string accountName(uint64_t id) {
  switch (id) {
  case pp::AccountIds::ID_GENESIS:
    return "the genesis account";
  case pp::AccountIds::ID_RESERVE:
    return "the reserve";
  case pp::AccountIds::ID_REGISTRAR:
    return "the registrar";
  case pp::AccountIds::ID_FEE:
    return "the fee account";
  case pp::AccountIds::ID_RECYCLE:
    return "the recycle account";
  default:
    return "account " + grouped(id);
  }
}

int64_t balanceOf(const pp::UserAccount &state, uint64_t token) {
  const auto it = state.wallet.mBalances.find(token);
  return it == state.wallet.mBalances.end() ? 0 : it->second;
}

op::Common commonFor(const ChainInfo &chain, const op::SignerSet &signers, const Making &making) {
  op::Common common;
  common.networkId = chain.networkId;
  common.genesisHash = chain.genesisHash;
  common.signers = signers;
  common.window = op::windowFrom(making.now, making.validForSeconds);
  common.notes = making.notes;
  common.createdAt = making.now;
  return common;
}

pp::Roe<void> checkMaking(const ChainInfo &chain, const op::SignerSet &signers, const Making &making) {
  const uint64_t longest = chain.meta.config.maxValidationTimespanSeconds;
  if (making.validForSeconds <= 0 || (longest > 0 && static_cast<uint64_t>(making.validForSeconds) > longest)) {
    return pp::Error("The chain lets an operation wait at most " + std::to_string(longest / 3600) + " hours");
  }
  if (!signers.account) {
    return pp::Error("No account signs it");
  }
  return {};
}

/** The least fee of a built operation's record, under the config in force. */
pp::Roe<uint64_t> leastFee(const ChainInfo &chain, const op::Operation &operation) {
  auto typed = operation.record.decode();
  if (!typed) {
    return pp::Error("The transaction does not decode: " + typed.error().message);
  }
  auto fee = pp::minimumFeeFor(chain.meta.config, typed.value());
  if (!fee) {
    return pp::Error("Cannot work out the fee: " + fee.error().message);
  }
  return fee.value();
}

/** Built with `fee`, refused below the least fee. */
pp::Roe<op::Operation> withFee(const ChainInfo &chain, pp::Roe<op::Operation> built, uint64_t fee) {
  if (!built) {
    return built;
  }
  auto least = leastFee(chain, built.value());
  if (!least) {
    return least.error();
  }
  if (fee < least.value()) {
    return pp::Error("The fee must be at least " + grouped(least.value()));
  }
  return built;
}

pp::Roe<void> checkNative(const pp::UserAccount &state, uint64_t needed, const std::string &what) {
  const int64_t native = balanceOf(state, pp::AccountIds::ID_GENESIS);
  if (native < 0 || static_cast<uint64_t>(native) < needed) {
    return pp::Error("The account holds " + grouped(native < 0 ? 0 : static_cast<uint64_t>(native)) +
                     " of the native token: not enough for " + what + " (" + grouped(needed) + ")");
  }
  return {};
}

TokenActivity describe(uint64_t account, const pp::Ledger::Record &record) {
  auto typed = record.decode();
  if (!typed) {
    return {"A transaction of type " + std::to_string(record.type), ""};
  }
  const auto &tx = typed.value();
  if (const auto *t = std::get_if<pp::Ledger::TxDefault>(&tx)) {
    const std::string token = t->tokenId == pp::AccountIds::ID_GENESIS ? ""
                              : t->tokenId == account                ? " of its token"
                                                                     : " of token " + grouped(t->tokenId);
    std::string what;
    if (t->fromWalletId == account && t->tokenId == account) {
      what = "Issued " + grouped(t->amount) + " to " + accountName(t->toWalletId);
    } else if (t->toWalletId == account && t->tokenId == account) {
      what = "Came back: " + grouped(t->amount) + " from " + accountName(t->fromWalletId);
    } else if (t->fromWalletId == account) {
      what = "Sent " + grouped(t->amount) + token + " to " + accountName(t->toWalletId);
    } else {
      what = "Received " + grouped(t->amount) + token + " from " + accountName(t->fromWalletId);
    }
    return {what, t->fromWalletId == account ? grouped(t->fee) : ""};
  }
  if (const auto *t = std::get_if<pp::Ledger::TxNewUser>(&tx)) {
    return {t->toWalletId == account ? "Created by " + accountName(t->fromWalletId)
                                     : "Created " + accountName(t->toWalletId),
            t->fromWalletId == account ? grouped(t->fee) : ""};
  }
  if (const auto *t = std::get_if<pp::Ledger::TxUserUpdate>(&tx)) {
    return {"Updated: its keys or its recovery", grouped(t->fee)};
  }
  if (const auto *t = std::get_if<pp::Ledger::TxAccountRecovery>(&tx)) {
    return {"Its recovery set announced new keys", grouped(t->fee)};
  }
  if (const auto *t = std::get_if<pp::Ledger::TxRenewal>(&tx)) {
    return {"Renewed", grouped(t->fee)};
  }
  if (std::holds_alternative<pp::Ledger::TxEndUser>(tx)) {
    return {"Ended: it could not pay its renewal", ""};
  }
  return {"A transaction of type " + std::to_string(record.type), ""};
}

} // namespace

pp::Roe<uint64_t> renewalFee(const ChainInfo &chain, uint64_t account, const pp::UserAccount &state) {
  pp::Ledger::TxRenewal probe;
  probe.walletId = account;
  probe.meta = state.ltsToString();
  auto fee = pp::minimumFeeFor(chain.meta.config, pp::Ledger::TypedTx(probe));
  if (!fee) {
    return pp::Error("Cannot work out the renewal fee: " + fee.error().message);
  }
  return fee.value();
}

pp::Roe<TokenOverview> tokenOverview(const ChainInfo &chain, uint64_t account, const pp::UserAccount &state,
                                     const std::vector<pp::Ledger::Record> &transactions) {
  if (!isIssuedId(account)) {
    return pp::Error("Account " + grouped(account) + " is not an issued account: tokens are issued by accounts " +
                     grouped(pp::AccountIds::ID_FIRST_ISSUED) + " to " + grouped(pp::AccountIds::ID_FIRST_USER - 1));
  }
  TokenOverview overview;
  overview.account = account;
  const int64_t own = balanceOf(state, account);
  overview.supply = own < 0 ? static_cast<uint64_t>(-(own + 1)) + 1 : 0;
  overview.native = balanceOf(state, pp::AccountIds::ID_GENESIS);
  auto fee = renewalFee(chain, account, state);
  if (!fee) {
    return fee.error();
  }
  overview.renewalFee = fee.value();
  const uint64_t keep = overview.renewalFee > std::numeric_limits<uint64_t>::max() / kRenewalsToKeep
                            ? std::numeric_limits<uint64_t>::max()
                            : overview.renewalFee * kRenewalsToKeep;
  overview.low = overview.native < 0 || static_cast<uint64_t>(overview.native) < keep;
  overview.minSignatures = state.wallet.minSignatures;
  overview.publicKeys = state.wallet.publicKeys;
  overview.recovery = state.wallet.recovery;
  overview.domain = state.wallet.domain;
  for (const auto &[token, balance] : state.wallet.mBalances) {
    if (token != account && token != pp::AccountIds::ID_GENESIS && balance != 0) {
      overview.otherTokens.emplace_back(token, balance);
    }
  }
  for (const auto &record : transactions) {
    if (overview.recent.size() == kRecentShown) {
      break;
    }
    overview.recent.push_back(describe(account, record));
  }
  return overview;
}

pp::Roe<op::Operation> makeIssue(const ChainInfo &chain, const op::SignerSet &issuer, const pp::UserAccount &state,
                                 uint64_t to, uint64_t amount, uint64_t fee, const Making &making) {
  if (auto ok = checkMaking(chain, issuer, making); !ok) {
    return ok.error();
  }
  const uint64_t account = *issuer.account;
  if (!isIssuedId(account)) {
    return pp::Error("Only an issued account issues a token");
  }
  if (auto ok = checkNotFrozen(state); !ok) {
    return ok.error();
  }
  if (to == account) {
    return pp::Error("Tokens are issued to another account");
  }
  if (amount == 0) {
    return pp::Error("Issue more than 0");
  }
  // The supply is minus the issuer's balance (an int64): it cannot pass the most that holds.
  const int64_t own = balanceOf(state, account);
  if (amount > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) ||
      own < std::numeric_limits<int64_t>::min() + static_cast<int64_t>(amount)) {
    return pp::Error("That would issue more than the chain can count");
  }
  if (auto ok = checkNative(state, fee, "the fee"); !ok) {
    return ok.error();
  }
  return withFee(chain, op::transfer(commonFor(chain, issuer, making), to, amount, fee, account), fee);
}

pp::Roe<op::Operation> makeSend(const ChainInfo &chain, const op::SignerSet &from, const pp::UserAccount &state,
                                uint64_t to, uint64_t token, uint64_t amount, uint64_t fee, const Making &making) {
  if (auto ok = checkMaking(chain, from, making); !ok) {
    return ok.error();
  }
  const uint64_t account = *from.account;
  if (token == account) {
    return makeIssue(chain, from, state, to, amount, fee, making);
  }
  if (auto ok = checkNotFrozen(state); !ok) {
    return ok.error();
  }
  if (to == account) {
    return pp::Error("It cannot go to the account it comes from");
  }
  if (amount == 0) {
    return pp::Error("Send more than 0");
  }
  if (token == pp::AccountIds::ID_GENESIS) {
    if (amount > std::numeric_limits<uint64_t>::max() - fee) {
      return pp::Error("That is more than the chain can count");
    }
    if (auto ok = checkNative(state, amount + fee, "the amount and the fee"); !ok) {
      return ok.error();
    }
  } else {
    const int64_t held = balanceOf(state, token);
    if (held < 0 || static_cast<uint64_t>(held) < amount) {
      return pp::Error("The account holds " + grouped(held < 0 ? 0 : static_cast<uint64_t>(held)) + " of token " +
                       grouped(token));
    }
    if (auto ok = checkNative(state, fee, "the fee"); !ok) {
      return ok.error();
    }
  }
  return withFee(chain, op::transfer(commonFor(chain, from, making), to, amount, fee, token), fee);
}

pp::Roe<uint64_t> keyReplacementFee(const ChainInfo &chain, const op::SignerSet &current, const pp::UserAccount &state,
                                    const std::vector<std::string> &keys, uint32_t minSignatures) {
  auto probe = op::replaceKeys(commonFor(chain, current, {0, 3600, ""}), state, keys, minSignatures, 0);
  if (!probe) {
    return probe.error();
  }
  return leastFee(chain, probe.value());
}

pp::Roe<op::Operation> makeKeyReplacement(const ChainInfo &chain, const op::SignerSet &current,
                                          const pp::UserAccount &state, const std::vector<std::string> &keys,
                                          uint32_t minSignatures, uint64_t fee, const Making &making) {
  if (auto ok = checkMaking(chain, current, making); !ok) {
    return ok.error();
  }
  if (*current.account < pp::AccountIds::ID_FIRST_ISSUED) {
    return pp::Error("A system account's keys are not replaced this way");
  }
  if (auto ok = checkNotFrozen(state); !ok) {
    return ok.error();
  }
  if (keys.empty()) {
    return pp::Error("Choose the keys from then on");
  }
  if (minSignatures == 0 || minSignatures > keys.size() || minSignatures > 255) {
    return pp::Error("Signatures needed: between 1 and the keys chosen");
  }
  std::vector<std::string> sortedNew = keys;
  std::vector<std::string> sortedNow = state.wallet.publicKeys;
  std::sort(sortedNew.begin(), sortedNew.end());
  std::sort(sortedNow.begin(), sortedNow.end());
  if (sortedNew == sortedNow && minSignatures == state.wallet.minSignatures) {
    return pp::Error("Those are the keys in force already");
  }
  if (auto ok = checkNative(state, fee, "the fee"); !ok) {
    return ok.error();
  }
  return withFee(chain, op::replaceKeys(commonFor(chain, current, making), state, keys, minSignatures, fee), fee);
}

namespace {

/** `make(fee)` built with the least fee it needs, within the account's native balance. */
template <typename Make>
pp::Roe<op::Operation> withLeastFee(const ChainInfo &chain, const pp::UserAccount &state, Make make) {
  auto probe = make(0);
  if (!probe) {
    return probe;
  }
  auto least = leastFee(chain, probe.value());
  if (!least) {
    return least.error();
  }
  if (auto ok = checkNative(state, least.value(), "the fee"); !ok) {
    return ok.error();
  }
  return make(least.value());
}

pp::Roe<void> checkRecoverable(uint64_t account) {
  if (account < pp::AccountIds::ID_FIRST_ISSUED) {
    return pp::Error("A system account's recovery is the genesis recovery set's");
  }
  return {};
}

/** A recovery key is none of the account's own: lost with them, it would recover nothing. */
pp::Roe<void> checkNotOwnKeys(const pp::UserAccount &state, const std::vector<std::string> &keys) {
  for (const auto &key : keys) {
    if (std::find(state.wallet.publicKeys.begin(), state.wallet.publicKeys.end(), key) !=
        state.wallet.publicKeys.end()) {
      return pp::Error("A recovery key is one of the account's own keys: lost or stolen with them, it recovers "
                       "nothing");
    }
  }
  return {};
}

/** The recovery set's signers for `account`, checked for making. */
pp::Roe<op::SignerSet> setSigners(const ChainInfo &chain, uint64_t account, const pp::UserAccount &state,
                                  const Making &making);

pp::Roe<op::SignerSet> setSigners(const ChainInfo &chain, uint64_t account, const pp::UserAccount &state,
                                  const Making &making) {
  if (auto ok = checkRecoverable(account); !ok) {
    return ok.error();
  }
  auto signers = recoverySignersOf(account, state);
  if (!signers) {
    return signers.error();
  }
  if (auto ok = checkMaking(chain, signers.value(), making); !ok) {
    return ok.error();
  }
  return signers;
}

} // namespace

pp::Roe<void> checkNotFrozen(const pp::UserAccount &state) {
  if (state.wallet.recovery.pending) {
    return pp::Error("The account is frozen: its recovery set announced new keys, taking over at epoch " +
                     grouped(state.wallet.recovery.applyEpoch) + "; only that set can withdraw it");
  }
  return {};
}

uint64_t epochSeconds(const ChainInfo &chain) {
  const auto &c = chain.meta.config;
  if (c.slotDuration == 0 || c.slotsPerEpoch > std::numeric_limits<uint64_t>::max() / c.slotDuration) {
    return 0;
  }
  return c.slotDuration * c.slotsPerEpoch;
}

uint64_t epochAt(const ChainInfo &chain, int64_t now) {
  const uint64_t epoch = epochSeconds(chain);
  const int64_t since = now - chain.meta.config.genesisTime;
  return epoch == 0 || since <= 0 ? 0 : static_cast<uint64_t>(since) / epoch;
}

pp::Roe<uint64_t> delaySecondsFor(const ChainInfo &chain, uint64_t days) {
  if (days == 0 || days > 3650) {
    return pp::Error("The delay: between 1 and 3,650 days");
  }
  const uint64_t seconds = days * 86400;
  const uint64_t least = pp::chain_tx::minRecoveryDelaySeconds(chain.meta.config);
  if (seconds < least) {
    return pp::Error("The chain's least delay is half an epoch: " + grouped(least) + " seconds");
  }
  return seconds;
}

pp::Roe<op::Operation> makeRecoverySet(const ChainInfo &chain, const op::SignerSet &current,
                                       const pp::UserAccount &state, const std::vector<std::string> &keys,
                                       uint32_t minSignatures, uint64_t delaySeconds, const Making &making) {
  if (auto ok = checkMaking(chain, current, making); !ok) {
    return ok.error();
  }
  if (auto ok = checkRecoverable(*current.account); !ok) {
    return ok.error();
  }
  if (auto ok = checkNotFrozen(state); !ok) {
    return ok.error();
  }
  if (state.wallet.recovery.hasSet()) {
    return pp::Error("The account has a recovery set: only that set changes it now");
  }
  if (keys.empty() || minSignatures == 0 || minSignatures > keys.size()) {
    return pp::Error("The recovery keys: at least one, and between 1 and that many to sign");
  }
  if (auto ok = checkNotOwnKeys(state, keys); !ok) {
    return ok.error();
  }
  return withLeastFee(chain, state, [&](uint64_t fee) {
    return op::setRecovery(commonFor(chain, current, making), state, keys, minSignatures, delaySeconds, fee);
  });
}

pp::Roe<op::SignerSet> recoverySignersOf(uint64_t account, const pp::UserAccount &state) {
  const auto &recovery = state.wallet.recovery;
  if (!recovery.hasSet()) {
    return pp::Error("Account " + grouped(account) + " has no recovery set");
  }
  op::SignerSet signers;
  signers.account = account;
  signers.publicKeys = recovery.publicKeys;
  signers.minSignatures = recovery.minSignatures;
  return signers;
}

pp::Roe<op::Operation> makeRecoveryAnnouncement(const ChainInfo &chain, uint64_t account, const pp::UserAccount &state,
                                                const std::vector<std::string> &keys, uint32_t minSignatures,
                                                const Making &making) {
  auto signers = setSigners(chain, account, state, making);
  if (!signers) {
    return signers.error();
  }
  if (state.wallet.recovery.pending) {
    return pp::Error("A recovery is announced already: it takes over at epoch " +
                     grouped(state.wallet.recovery.applyEpoch) + ", unless the set withdraws it");
  }
  if (keys.empty() || minSignatures == 0 || minSignatures > keys.size()) {
    return pp::Error("The new keys: at least one, and between 1 and that many to sign");
  }
  return withLeastFee(chain, state, [&](uint64_t fee) {
    return op::announceRecovery(commonFor(chain, signers.value(), making), account, keys, minSignatures, fee);
  });
}

pp::Roe<op::Operation> makeRecoveryWithdrawal(const ChainInfo &chain, uint64_t account, const pp::UserAccount &state,
                                              const Making &making) {
  auto signers = setSigners(chain, account, state, making);
  if (!signers) {
    return signers.error();
  }
  if (!state.wallet.recovery.pending) {
    return pp::Error("No recovery is announced for account " + grouped(account));
  }
  return withLeastFee(chain, state, [&](uint64_t fee) {
    return op::withdrawRecovery(commonFor(chain, signers.value(), making), account, fee);
  });
}

pp::Roe<op::Operation> makeRecoveryChange(const ChainInfo &chain, uint64_t account, const pp::UserAccount &state,
                                          const std::vector<std::string> &keys, uint32_t minSignatures,
                                          uint64_t delaySeconds, const Making &making) {
  auto signers = setSigners(chain, account, state, making);
  if (!signers) {
    return signers.error();
  }
  if (state.wallet.recovery.pending) {
    return pp::Error("A recovery is announced: withdraw it before changing the set");
  }
  if (!keys.empty()) {
    if (minSignatures == 0 || minSignatures > keys.size()) {
      return pp::Error("Signatures to act: between 1 and the recovery keys chosen");
    }
    if (auto ok = checkNotOwnKeys(state, keys); !ok) {
      return ok.error();
    }
  }
  return withLeastFee(chain, state, [&](uint64_t fee) {
    return op::changeRecovery(commonFor(chain, signers.value(), making), account, keys, minSignatures, delaySeconds,
                              fee);
  });
}

} // namespace apps::net
