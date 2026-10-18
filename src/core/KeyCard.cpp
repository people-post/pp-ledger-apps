#include "core/KeyCard.h"

#include "core/AtomicFile.h"
#include "lib/common/Utilities.h"
#include "operation/Operation.h"
#include "common/io/Json.h"

namespace admin {

using pp::common::Object;

std::string toJson(const KeyCard &card) {
  Object root;
  root.set("format", kKeyCardFormat);
  root.setJsonUInt("version", kKeyCardVersion);
  root.set("name", card.name);
  root.set("role", roleName(card.role));
  root.set("publicKey", pp::utl::hexEncode(card.publicKey));
  root.set("fingerprint", pp::operation::fingerprint(card.publicKey));
  return pp::common::io::objectToJsonString(root, 2) + "\n";
}

pp::Roe<KeyCard> keyCardFromJson(const std::string &json) {
  Object root;
  if (!pp::common::io::objectFromJsonString(root, json) || root.getString("format") != kKeyCardFormat) {
    return pp::Error("Not a key card");
  }
  if (root.getNonNegInt("version") != kKeyCardVersion) {
    return pp::Error("A key card of a version this app does not read");
  }
  KeyCard card;
  card.name = root.getString("name").value_or("");
  const auto role = roleFromName(root.getString("role").value_or(""));
  card.publicKey = pp::utl::hexDecode(root.getString("publicKey").value_or(""));
  if (card.name.empty() || !role || card.publicKey.size() != pp::utl::kMlDsaPublicKeyBytes) {
    return pp::Error("The key card is damaged");
  }
  card.role = *role;
  if (root.getString("fingerprint") != pp::operation::fingerprint(card.publicKey)) {
    return pp::Error("The key card's fingerprint is not its key's: it was changed");
  }
  return card;
}

pp::Roe<KeyCard> loadKeyCard(const std::string &path) {
  auto json = readFile(path);
  if (!json) {
    return json.error();
  }
  auto card = keyCardFromJson(json.value());
  if (!card) {
    return pp::Error(path + ": " + card.error().message);
  }
  return card;
}

pp::Roe<void> saveKeyCard(const KeyCard &card, const std::string &path) { return writeNewFile(path, toJson(card)); }

} // namespace admin
