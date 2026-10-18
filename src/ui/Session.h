#pragma once
// What the screens of an app share: the person's vault, and how to move between screens.

#include "core/Vault.h"
#include "operation/Operation.h"

#include <functional>
#include <utility>
#include <vector>
#include <string>

namespace apps::ui_host {

struct Session {
  explicit Session(std::string vaultDir) : vault(std::move(vaultDir)) {}

  Vault vault;
  /** Go to a screen by name ("vault", "review", "keys"); the app decides what they are. */
  std::function<void(const std::string &)> navigate;
  /** How a submission goes: progress, then done (in a block) or failed. */
  struct SubmitProgress {
    std::string status;
    bool done{false};
    bool failed{false};
  };
  /**
   * Role apps: send a complete operation to the chain and follow it into a block, reporting on the
   * UI thread. Empty in the Signer, which has no network (AD005).
   */
  std::function<void(const pp::operation::Operation &, std::function<void(const SubmitProgress &)>)> submit;

  /**
   * Role apps: what `operation` would change, against the chain now (a config update's settings
   * and admin set, a certificate's generation), as label/value lines, on the UI thread; the error
   * as a single line. Empty in the Signer: offline, it shows the operation alone.
   */
  std::function<void(const pp::operation::Operation &,
                     std::function<void(std::vector<std::pair<std::string, std::string>>)>)>
      compare;

  /** Why the vault was locked, for the vault screen to say ("" when the person locked it). */
  std::string lockedBecause;

  /** Minutes without use after which an app locks the vault. */
  static constexpr int kIdleLockMinutes = 10;
};

} // namespace apps::ui_host
