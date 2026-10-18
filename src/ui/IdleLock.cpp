#include "ui/IdleLock.h"

#include <cstdlib>
#include <string>

namespace admin::ui_host {

void installIdleLock(UiApp &app, Session &session) {
  int idleSeconds = Session::kIdleLockMinutes * 60;
#ifdef PP_ADMIN_DEV_HOOKS
  if (const char *seconds = std::getenv("PP_ADMIN_DEV_IDLE_SECONDS")) {
    idleSeconds = std::atoi(seconds);
  }
#endif
  app.setIdleTimeout(idleSeconds, [&session, idleSeconds] {
    if (!session.vault.isUnlocked()) {
      return;
    }
    session.vault.lock();
    session.lockedBecause = "Locked after " +
                            (idleSeconds % 60 == 0 ? std::to_string(idleSeconds / 60) + " minutes"
                                                   : std::to_string(idleSeconds) + " seconds") +
                            " without use.";
    session.navigate("vault");
  });
}

} // namespace admin::ui_host
