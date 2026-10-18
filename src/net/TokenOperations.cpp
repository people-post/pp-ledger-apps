#include "net/TokenOperations.h"

#include "chain/RecordHandler.h"
#include "client/AccountIds.h"

#include <algorithm>
#include <limits>

namespace admin::net {

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
    return {"Its keys replaced", grouped(t->fee)};
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

} // namespace admin::net
