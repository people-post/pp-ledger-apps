#pragma once
// Every app locks the vault after Session::kIdleLockMinutes without use, and says why.

#include "ui/Session.h"
#include "ui/UiApp.h"

namespace admin::ui_host {

/** Lock `session`'s vault when nobody has touched `app` for a while (development builds: $PP_ADMIN_DEV_IDLE_SECONDS). */
void installIdleLock(UiApp &app, Session &session);

} // namespace admin::ui_host
