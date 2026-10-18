#pragma once
// What the screens of an app share: the person's vault, and how to move between screens.

#include "core/Vault.h"

#include <functional>
#include <string>

namespace admin::ui_host {

struct Session {
  explicit Session(std::string vaultDir) : vault(std::move(vaultDir)) {}

  Vault vault;
  /** Go to a screen by name ("vault", "review", "keys"); the app decides what they are. */
  std::function<void(const std::string &)> navigate;
  /** Why the vault was locked, for the vault screen to say ("" when the person locked it). */
  std::string lockedBecause;

  /** Minutes without use after which an app locks the vault. */
  static constexpr int kIdleLockMinutes = 10;
};

} // namespace admin::ui_host
