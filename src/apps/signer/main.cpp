// pp-ledger-signer: every key holder's app. Opens an operation, shows what it does, signs it with a
// key from the vault — offline. Usage: pp-ledger-signer [operation file]
#include "ui/IdleLock.h"
#include "ui/KeysScreen.h"
#include "ui/Nav.h"
#include "ui/ReviewScreen.h"
#include "ui/UiApp.h"
#include "ui/VaultScreen.h"

#include "core/Vault.h"

#include <SDL3/SDL_main.h>

#include <cstdlib>

using namespace apps;
using namespace apps::ui_host;

int main(int argc, char **argv) {
  std::vector<std::string> args;
  AppOptions options = UiApp::parseArgs(argc, argv, {.title = "Signer — PP Ledger"}, args);
  UiApp app(options);
  if (!app.start()) {
    return 1;
  }
  Session session(Vault::defaultDir());
  Nav nav(session);
  VaultScreen vault(session);
  ReviewScreen review(session);
  KeysScreen keys(session);
  if (!nav.build(app.context(), "Signer", {{"review", "Review and sign"}, {"keys", "Keys and people"}}) || !vault.build(app) || !review.build(app) || !keys.build(app)) {
    return 1;
  }
  session.navigate = [&](const std::string &screen) {
    if (!session.vault.isUnlocked() || screen == "vault") {
      app.show(vault);
      return;
    }
    nav.select(screen);
    if (screen == "keys") {
      app.show(keys);
    } else {
      app.show(review);
    }
  };
  installIdleLock(app, session);
  std::string first = "vault";
#ifdef PP_APPS_DEV_HOOKS
  // Development only: unattended unlock and a first screen, for screenshots.
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == "--show") {
      first = args[i + 1];
      args.erase(args.begin() + static_cast<long>(i), args.begin() + static_cast<long>(i) + 2);
      break;
    }
  }
  if (const char *passphrase = std::getenv("PP_APPS_DEV_PASSPHRASE")) {
    (void)session.vault.unlock(passphrase);
  }
#endif
  if (!args.empty()) {
    review.open(args.front()); // shown once the vault is unlocked
  }
  session.navigate(first);
  return app.run();
}
