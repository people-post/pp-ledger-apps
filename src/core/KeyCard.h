#pragma once
// Key cards: a person's public key, named and with its role, exchanged as a file — how participants
// tell each other their keys (the genesis plan, an address book). DESIGN.md § The genesis ceremony.

#include "core/Roles.h"
#include "common/Error.h"

#include <string>

namespace apps {

inline constexpr const char *kKeyCardFormat = "pp-ledger-apps-keycard";
inline constexpr uint32_t kKeyCardVersion = 1;

struct KeyCard {
  std::string name;
  Role role{Role::Admin};
  /** Raw ML-DSA-65 public key. */
  std::string publicKey;

  bool operator==(const KeyCard &other) const = default;
};

/** JSON: format, version, name, role, publicKey (hex), fingerprint (what people compare). */
std::string toJson(const KeyCard &card);
/** Refuses an unknown format or role, a malformed key, or a fingerprint that is not the key's. */
pp::Roe<KeyCard> keyCardFromJson(const std::string &json);
pp::Roe<KeyCard> loadKeyCard(const std::string &path);
/** Writes a new file (refuses an existing one). */
pp::Roe<void> saveKeyCard(const KeyCard &card, const std::string &path);

} // namespace apps
