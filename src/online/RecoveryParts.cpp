#include "online/RecoveryParts.h"

#include "core/Amounts.h"
#include "operation/Operation.h"

#include <algorithm>

namespace apps::online {

std::string ChosenKeys::add(const std::string &name, const std::string &publicKey) {
  if (std::find(keys.begin(), keys.end(), publicKey) != keys.end()) {
    return "That key is chosen already.";
  }
  rows.push_back({name, pp::operation::fingerprint(publicKey)});
  keys.push_back(publicKey);
  return "";
}

void ChosenKeys::drop(int index) {
  if (index < 0 || index >= static_cast<int>(keys.size())) {
    return;
  }
  rows.erase(rows.begin() + index);
  keys.erase(keys.begin() + index);
}

void ChosenKeys::clear() {
  rows.clear();
  keys.clear();
}

std::string aboutTime(uint64_t seconds) {
  if (seconds < 3600) {
    return "within the hour";
  }
  if (seconds < 2 * 86400) {
    return "about " + std::to_string((seconds + 1800) / 3600) + " hours";
  }
  return "about " + std::to_string((seconds + 43200) / 86400) + " days";
}

std::string fingerprintsText(const std::vector<std::string> &keys) {
  std::string fingerprints;
  for (const auto &key : keys) {
    fingerprints += (fingerprints.empty() ? "" : ", ") + pp::operation::fingerprint(key);
  }
  return fingerprints;
}

std::string keysText(const std::vector<std::string> &keys, uint32_t minSignatures) {
  return std::to_string(minSignatures) + " of " + std::to_string(keys.size()) + " (" + fingerprintsText(keys) + ")";
}

std::string recoveryText(const pp::AccountRecovery &r, uint64_t epochSeconds) {
  if (!r.hasSet()) {
    return "";
  }
  return std::to_string(r.minSignatures) + " of " + std::to_string(r.publicKeys.size()) +
         " recovery keys; new keys wait " + formatAmount(r.delayEpochs) + " epochs" +
         (epochSeconds ? " (" + aboutTime(r.delayEpochs * epochSeconds) + ")" : "");
}

std::string announcedText(const pp::AccountRecovery &r, uint64_t tipEpoch, uint64_t epochSeconds,
                          const std::string &whereToCancel) {
  if (!r.pending) {
    return "";
  }
  const uint64_t left = r.applyEpoch > tipEpoch ? r.applyEpoch - tipEpoch : 0;
  return "Its recovery set announced new keys, " + keysText(r.nextPublicKeys, r.nextMinSignatures) + ", at block " +
         formatAmount(r.announcedAt) + ". They take over at epoch " + formatAmount(r.applyEpoch) +
         (epochSeconds ? " — " + aboutTime(left * epochSeconds) + " from now —" : "") +
         " unless the account's keys cancel it." +
         (whereToCancel.empty() ? "" : " If you did not ask for it, cancel it now (" + whereToCancel + ").");
}

} // namespace apps::online
