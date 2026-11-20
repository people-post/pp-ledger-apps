#pragma once
// The people whose keys this person deals with, by name: from key cards, kept in the vault (names
// and keys are nobody else's business). Apps show names, not key hex (DESIGN.md § The genesis
// ceremony).

#include "core/KeyCard.h"
#include "core/Vault.h"

#include <optional>
#include <string>
#include <vector>

namespace apps {

class AddressBook {
public:
  /** The vault file it is kept in. */
  static constexpr const char *kFileName = "address-book.json";

  /** The book in `vault` (empty when none yet). */
  static pp::Roe<AddressBook> load(const Vault &vault);
  pp::Roe<void> save(Vault &vault) const;

  const std::vector<KeyCard> &cards() const { return cards_; }
  /** Add `card`, or rename and re-role the card of the same key. */
  void put(const KeyCard &card);
  /** Forget the card of `publicKey`; whether there was one. */
  bool remove(const std::string &publicKey);
  std::optional<KeyCard> find(const std::string &publicKey) const;

private:
  std::vector<KeyCard> cards_;
};

} // namespace apps
