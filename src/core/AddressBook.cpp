#include "core/AddressBook.h"

#include "ledger/lib/common/Utilities.h"
#include "common/io/Json.h"

#include <algorithm>

namespace apps {

using pp::common::Object;
using pp::common::Value;

pp::Roe<AddressBook> AddressBook::load(const Vault &vault) {
  auto json = vault.readFile(kFileName);
  if (!json) {
    return json.error();
  }
  AddressBook book;
  if (json.value().empty()) {
    return book;
  }
  Object root;
  if (!pp::common::io::objectFromJsonString(root, json.value())) {
    return pp::Error("The address book is damaged");
  }
  if (const auto *cards = root.getArray("cards")) {
    for (const auto &value : cards->elements) {
      const Object *entry = pp::common::asObject(value);
      const auto role = entry ? roleFromName(entry->getString("role").value_or("")) : std::nullopt;
      KeyCard card;
      card.name = entry ? entry->getString("name").value_or("") : "";
      card.publicKey = entry ? pp::utl::hexDecode(entry->getString("publicKey").value_or("")) : "";
      if (!role || card.name.empty() || card.publicKey.size() != pp::utl::kMlDsaPublicKeyBytes) {
        return pp::Error("The address book is damaged");
      }
      card.role = *role;
      book.cards_.push_back(std::move(card));
    }
  }
  return book;
}

pp::Roe<void> AddressBook::save(Vault &vault) const {
  std::vector<Value> cards;
  for (const auto &card : cards_) {
    auto entry = std::make_shared<Object>();
    entry->set("name", card.name);
    entry->set("role", roleName(card.role));
    entry->set("publicKey", pp::utl::hexEncode(card.publicKey));
    cards.emplace_back(std::move(entry));
  }
  Object root;
  root.set("cards", Object::array(std::move(cards)));
  return vault.writeFile(kFileName, pp::common::io::objectToJsonString(root));
}

void AddressBook::put(const KeyCard &card) {
  auto it = std::find_if(cards_.begin(), cards_.end(),
                         [&](const KeyCard &c) { return c.publicKey == card.publicKey; });
  if (it != cards_.end()) {
    *it = card;
  } else {
    cards_.push_back(card);
  }
}

bool AddressBook::remove(const std::string &publicKey) {
  const auto before = cards_.size();
  std::erase_if(cards_, [&](const KeyCard &c) { return c.publicKey == publicKey; });
  return cards_.size() != before;
}

std::optional<KeyCard> AddressBook::find(const std::string &publicKey) const {
  for (const auto &card : cards_) {
    if (card.publicKey == publicKey) {
      return card;
    }
  }
  return std::nullopt;
}

} // namespace apps
