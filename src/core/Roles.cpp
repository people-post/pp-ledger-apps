#include "core/Roles.h"

#include <utility>

namespace admin {

namespace {

const std::vector<std::pair<Role, std::pair<const char *, const char *>>> &names() {
  static const std::vector<std::pair<Role, std::pair<const char *, const char *>>> table = {
      {Role::Admin, {"admin", "Admin"}},       {Role::Recovery, {"recovery", "Recovery"}},
      {Role::Reserve, {"reserve", "Reserve"}}, {Role::Registrar, {"registrar", "Registrar"}},
      {Role::Fee, {"fee", "Fee"}},             {Role::Recycle, {"recycle", "Recycle"}},
      {Role::Beacon, {"beacon", "Beacon"}},    {Role::Miner, {"miner", "Miner"}},
      {Role::Owner, {"owner", "Account owner"}}};
  return table;
}

} // namespace

std::string roleName(Role role) {
  for (const auto &[r, n] : names()) {
    if (r == role) {
      return n.first;
    }
  }
  return "";
}

std::optional<Role> roleFromName(const std::string &name) {
  for (const auto &[r, n] : names()) {
    if (name == n.first) {
      return r;
    }
  }
  return std::nullopt;
}

std::string roleTitle(Role role) {
  for (const auto &[r, n] : names()) {
    if (r == role) {
      return n.second;
    }
  }
  return "";
}

const std::vector<Role> &vaultRoles() {
  static const std::vector<Role> roles = {Role::Admin, Role::Recovery, Role::Reserve,
                                          Role::Registrar, Role::Fee, Role::Recycle};
  return roles;
}

} // namespace admin
