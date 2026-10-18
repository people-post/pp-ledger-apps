#pragma once
// An account's operations, for a token issuer (projects/token-app/DESIGN.md) and the wallet: the issuer is an issued account, its
// token the one with its own id (TA004). Issuing is a transfer of that token out of the account (its
// balance of it goes negative: minus the supply); sending moves the native token or the issuer's; a
// key replacement is the account's own update. Built against the chain as a node shows it and
// checked before anyone signs, like the admin set's (AdminOperations).

#include "net/AdminOperations.h"

#include <cstdint>
#include <string>
#include <vector>

namespace apps::net {

/** One of the issuer's transactions, as the overview lists it. */
struct TokenActivity {
  std::string what; ///< "Issued 1,000 to account 1,073,741,831", "Renewal", …
  std::string fee;  ///< "1" (native), "" when none
};

/** What the issuer's operators watch (TA005). */
struct TokenOverview {
  uint64_t account{0};
  /** Tokens issued and not come back: minus the account's balance of its own token. */
  uint64_t supply{0};
  /** The native balance (fees and renewals are paid from it). */
  int64_t native{0};
  /** About what a renewal costs: the fee for the account's metadata under the config in force. */
  uint64_t renewalFee{0};
  /** The native balance covers fewer renewals than kRenewalsToKeep. */
  bool low{false};
  uint32_t minSignatures{0};
  std::vector<std::string> publicKeys;
  /** Its recovery set and any recovery announced (pp-ledger projects/account-recovery). */
  pp::AccountRecovery recovery;
  /** Other tokens the account holds: id and balance. */
  std::vector<std::pair<uint64_t, int64_t>> otherTokens;
  /** Latest first, at most kRecentShown. */
  std::vector<TokenActivity> recent;
};

/** The renewals the native balance should cover before the overview warns. */
constexpr uint64_t kRenewalsToKeep = 5;

/** The transactions the overview lists, at most. */
constexpr size_t kRecentShown = 20;

/** The overview of issuer `account`, from its account and its `transactions` (latest first). */
pp::Roe<TokenOverview> tokenOverview(const ChainInfo &chain, uint64_t account, const pp::UserAccount &state,
                                     const std::vector<pp::Ledger::Record> &transactions);

/** About what a renewal of `state` costs under the config in force. */
pp::Roe<uint64_t> renewalFee(const ChainInfo &chain, uint64_t account, const pp::UserAccount &state);

/**
 * New tokens of issuer `issuer` (its signers) to account `to`: a transfer of the token whose id is
 * the issuer's. The native balance pays the fee.
 */
pp::Roe<pp::operation::Operation> makeIssue(const ChainInfo &chain, const pp::operation::SignerSet &issuer,
                                            const pp::UserAccount &state, uint64_t to, uint64_t amount, uint64_t fee,
                                            const Making &making);

/**
 * A transfer of `token` (0: native) from the account (`from`, its signers) to `to`, within what it
 * holds — but for its own token, which it issues (makeIssue).
 */
pp::Roe<pp::operation::Operation> makeSend(const ChainInfo &chain, const pp::operation::SignerSet &from,
                                           const pp::UserAccount &state, uint64_t to, uint64_t token,
                                           uint64_t amount, uint64_t fee, const Making &making);

/**
 * The account's keys and threshold replaced (T_USER_UPDATE), signed by the keys in force. The rest
 * of the account stays as the chain has it; the fee leaves its native balance.
 */
pp::Roe<pp::operation::Operation> makeKeyReplacement(const ChainInfo &chain, const pp::operation::SignerSet &current,
                                                     const pp::UserAccount &state,
                                                     const std::vector<std::string> &keys, uint32_t minSignatures,
                                                     uint64_t fee, const Making &making);
/** The least fee of a key replacement to `keys`. */
pp::Roe<uint64_t> keyReplacementFee(const ChainInfo &chain, const pp::operation::SignerSet &current,
                                    const pp::UserAccount &state, const std::vector<std::string> &keys,
                                    uint32_t minSignatures);

// ---- Account recovery (pp-ledger projects/account-recovery) ----
// An account's recovery set stands above its keys: it announces the account's next keys — the
// account frozen until they take over, at the first epoch start after its delay — withdraws that, or
// changes itself. The account's keys name the first set. Each of these pays the least fee.

/** Refused while the account is frozen: its recovery set announced new keys. */
pp::Roe<void> checkNotFrozen(const pp::UserAccount &state);

/** Seconds an epoch lasts under the config in force (0 when the config does not say). */
uint64_t epochSeconds(const ChainInfo &chain);
/** The epoch at Unix time `now`, by the chain's clock (its genesis time and epoch length). */
uint64_t epochAt(const ChainInfo &chain, int64_t now);
/** The delay of `days`, in seconds; refused below the chain's least (half an epoch). */
pp::Roe<uint64_t> delaySecondsFor(const ChainInfo &chain, uint64_t days);

/**
 * The account's first recovery set (`keys`, `minSignatures` of them to act) and delay, named by the
 * keys in force (`current`). Its keys stay. Refused once it has a set: only the set changes it.
 */
pp::Roe<pp::operation::Operation> makeRecoverySet(const ChainInfo &chain, const pp::operation::SignerSet &current,
                                                  const pp::UserAccount &state,
                                                  const std::vector<std::string> &keys, uint32_t minSignatures,
                                                  uint64_t delaySeconds, const Making &making);
/** Who signs for account `account`'s recovery set: its keys and threshold, as `state` holds them. */
pp::Roe<pp::operation::SignerSet> recoverySignersOf(uint64_t account, const pp::UserAccount &state);
/**
 * Account `account`'s next keys (`keys`, `minSignatures` of them), announced by its recovery set: the
 * account is frozen until they take over. The account pays the fee.
 */
pp::Roe<pp::operation::Operation> makeRecoveryAnnouncement(const ChainInfo &chain, uint64_t account,
                                                           const pp::UserAccount &state,
                                                           const std::vector<std::string> &keys,
                                                           uint32_t minSignatures, const Making &making);
/** The announcement withdrawn by the recovery set: the account's keys sign again. */
pp::Roe<pp::operation::Operation> makeRecoveryWithdrawal(const ChainInfo &chain, uint64_t account,
                                                         const pp::UserAccount &state, const Making &making);
/** The recovery set replaced by itself (`keys`, `minSignatures`, `delaySeconds`), or removed (no keys). */
pp::Roe<pp::operation::Operation> makeRecoveryChange(const ChainInfo &chain, uint64_t account,
                                                     const pp::UserAccount &state,
                                                     const std::vector<std::string> &keys, uint32_t minSignatures,
                                                     uint64_t delaySeconds, const Making &making);

} // namespace apps::net
