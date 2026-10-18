#pragma once
// What the wallet's and the token app's account recovery share (pp-ledger projects/account-recovery):
// keys chosen from key cards, and an account's recovery as people read it.

#include "client/Client.h"

#include <cstdint>
#include <string>
#include <vector>

namespace apps::online {

/** Keys chosen from key cards (or made here), shown by name and fingerprint. */
struct ChosenKeys {
  struct Row {
    std::string name;
    std::string fingerprint;
  };
  std::vector<Row> rows;
  std::vector<std::string> keys;

  /** Adds the key; "" or why not (already chosen). */
  std::string add(const std::string &name, const std::string &publicKey);
  void drop(int index);
  void clear();
};

/** "within the hour", "about 5 hours", "about 7 days". */
std::string aboutTime(uint64_t seconds);
/** The keys' fingerprints, comma-separated. */
std::string fingerprintsText(const std::vector<std::string> &keys);
/** "2 of 3 (fingerprints)". */
std::string keysText(const std::vector<std::string> &keys, uint32_t minSignatures);
/** An account's recovery set and delay ("2 of 3 recovery keys; …"): "" when none. `epochSeconds` 0: unknown. */
std::string recoveryText(const pp::AccountRecovery &recovery, uint64_t epochSeconds);
/**
 * A recovery announced, and where its owner cancels it (`whereToCancel`: "Recovery…", "Signers"; ""
 * for whoever announced it): "" when none.
 * `tipEpoch`: the epoch now.
 */
std::string announcedText(const pp::AccountRecovery &recovery, uint64_t tipEpoch, uint64_t epochSeconds,
                          const std::string &whereToCancel);

} // namespace apps::online
