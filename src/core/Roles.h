#pragma once
// What a key is for: the chain's system accounts and key sets (DESIGN.md § Who uses them).

#include <optional>
#include <string>
#include <vector>

namespace admin {

enum class Role {
  Admin,     ///< the genesis account: minting, config updates, beacon certificates
  Recovery,  ///< the recovery set: replaces the admin keys
  Reserve,   ///< the treasury
  Registrar, ///< creates issued accounts
  Fee,       ///< collects fees
  Recycle,   ///< takes ended accounts' balances
  Beacon,    ///< the beacon's signing key (on key cards only: the beacon holds it)
  Miner,     ///< a genesis miner's key (on key cards only: the miner holds it)
  Owner,     ///< an account owner's key: the registrar creates their account (a token issuer's operators)
};

/** The name in files ("admin", "reserve", …). */
std::string roleName(Role role);
std::optional<Role> roleFromName(const std::string &name);
/** As people read it ("Admin", "Reserve", …). */
std::string roleTitle(Role role);
/** The roles a vault holds keys for (not the beacon's nor a miner's). */
const std::vector<Role> &vaultRoles();

} // namespace admin
