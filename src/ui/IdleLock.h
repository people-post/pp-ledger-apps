#pragma once
// Every app locks the vault after Session::kIdleLockMinutes without use, and says why.

#include "ui/Session.h"
#include "ui/UiApp.h"

namespace apps::ui_host {

/** Lock `session`'s vault when nobody has touched `app` for a while (development builds: $PP_APPS_DEV_IDLE_SECONDS). */
void installIdleLock(UiApp &app, Session &session);

} // namespace apps::ui_host
