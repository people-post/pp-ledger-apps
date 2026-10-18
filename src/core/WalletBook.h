#pragma once
// The wallet's records (projects/wallet-app/DESIGN.md): the network card it pinned — the anchor every
// verified read starts from (WL003) — and the accounts the person follows. Kept in the vault.

#include "core/Vault.h"
#include "common/Error.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace apps::wallet {

/**
 * A network as its operators publish it: its id, block 0's hash (raw, 32 bytes) and a few nodes'
 * addresses. File: JSON, format pp-ledger-network-card, version 1, the hash in hex.
 */
struct NetworkCard {
  std::string networkId;
  std::string genesisHash;
  std::vector<std::string> nodes;
  std::string notes;

  bool operator==(const NetworkCard &) const = default;
};

inline constexpr const char *kNetworkCardFormat = "pp-ledger-network-card";
std::string networkCardToJson(const NetworkCard &card);
pp::Roe<NetworkCard> networkCardFromJson(const std::string &json);
/** Block 0's hash as people compare it: hex, in groups of eight. */
std::string hashForPeople(const std::string &hash);

/** An account the person follows: its number, and a name of theirs for it. */
struct FollowedAccount {
  uint64_t id{0};
  std::string label;

  bool operator==(const FollowedAccount &) const = default;
};

struct WalletBook {
  static constexpr const char *kFileName = "wallet.json";

  /** The network pinned: none until the person pins one. */
  std::optional<NetworkCard> card;
  std::vector<FollowedAccount> accounts;

  static pp::Roe<WalletBook> load(const Vault &vault);
  pp::Roe<void> save(Vault &vault) const;
};

} // namespace apps::wallet
