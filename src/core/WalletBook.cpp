#include "core/WalletBook.h"

#include "lib/common/Utilities.h"
#include "common/io/Json.h"

#include <memory>

namespace apps::wallet {

using pp::common::Object;
using pp::common::Value;

namespace {

constexpr uint32_t kCardVersion = 1;

std::shared_ptr<Object> cardObject(const NetworkCard &card) {
  auto o = std::make_shared<Object>();
  o->set("format", kNetworkCardFormat);
  o->setJsonUInt("version", kCardVersion);
  o->set("networkId", card.networkId);
  o->set("genesisHash", pp::utl::hexEncode(card.genesisHash));
  std::vector<Value> nodes;
  for (const auto &node : card.nodes) {
    nodes.emplace_back(node);
  }
  o->set("nodes", Object::array(std::move(nodes)));
  o->set("notes", card.notes);
  return o;
}

pp::Roe<NetworkCard> readCard(const Object &o) {
  if (o.getString("format").value_or("") != kNetworkCardFormat) {
    return pp::Error("Not a network card");
  }
  if (o.getNonNegInt("version").value_or(0) != kCardVersion) {
    return pp::Error("A network card of another version");
  }
  NetworkCard card;
  card.networkId = o.getString("networkId").value_or("");
  std::string hex = o.getString("genesisHash").value_or("");
  if (hex.rfind("0x", 0) == 0) {
    hex = hex.substr(2);
  }
  card.genesisHash = pp::utl::hexDecode(hex);
  if (card.networkId.empty() || card.genesisHash.size() != 32) {
    return pp::Error("The network card names no network, or no 32-byte genesis hash");
  }
  if (const auto *nodes = o.getArray("nodes")) {
    for (const auto &value : nodes->elements) {
      auto node = pp::common::asString(value);
      if (!node || node->empty() || node->front() != '/') {
        return pp::Error("The network card has a node address that is not one (it starts with /ip4/ or /dns4/)");
      }
      card.nodes.push_back(*node);
    }
  }
  card.notes = o.getString("notes").value_or("");
  return card;
}

} // namespace

std::string networkCardToJson(const NetworkCard &card) {
  return pp::common::io::objectToJsonString(*cardObject(card), 2) + "\n";
}

pp::Roe<NetworkCard> networkCardFromJson(const std::string &json) {
  Object root;
  if (!pp::common::io::objectFromJsonString(root, json)) {
    return pp::Error("Not a network card: not JSON");
  }
  return readCard(root);
}

std::string hashForPeople(const std::string &hash) {
  const std::string hex = pp::utl::hexEncode(hash);
  std::string out;
  for (size_t i = 0; i < hex.size(); i += 8) {
    out += (i ? " " : "") + hex.substr(i, 8);
  }
  return out;
}

pp::Roe<WalletBook> WalletBook::load(const Vault &vault) {
  auto json = vault.readFile(kFileName);
  if (!json) {
    return json.error();
  }
  WalletBook book;
  if (json.value().empty()) {
    return book;
  }
  Object root;
  if (!pp::common::io::objectFromJsonString(root, json.value())) {
    return pp::Error("The wallet's records are damaged");
  }
  if (const Object *card = root.getObject("card")) {
    auto read = readCard(*card);
    if (!read) {
      return pp::Error("The pinned network card: " + read.error().message);
    }
    book.card = std::move(read.value());
  }
  if (const auto *accounts = root.getArray("accounts")) {
    for (const auto &value : accounts->elements) {
      const Object *e = pp::common::asObject(value);
      const auto id = e ? e->getNonNegInt("id") : std::nullopt;
      if (!id) {
        return pp::Error("The wallet's account list is damaged");
      }
      book.accounts.push_back({*id, e->getString("label").value_or("")});
    }
  }
  return book;
}

pp::Roe<void> WalletBook::save(Vault &vault) const {
  Object root;
  if (card) {
    root.set("card", *cardObject(*card));
  }
  std::vector<Value> list;
  for (const auto &a : accounts) {
    auto e = std::make_shared<Object>();
    e->setJsonUInt("id", a.id);
    e->set("label", a.label);
    list.emplace_back(std::move(e));
  }
  root.set("accounts", Object::array(std::move(list)));
  return vault.writeFile(kFileName, pp::common::io::objectToJsonString(root));
}

} // namespace apps::wallet
