// pp-admin-reserve: for the reserve's holders. Make a transfer out of the treasury, sign it, collect
// the others' signatures, submit it and see it land. Usage: pp-admin-reserve [operation file]
#include "online/NetworkScreen.h"
#include "online/Online.h"
#include "online/TransferScreen.h"
#include "ui/KeysScreen.h"
#include "ui/Nav.h"
#include "ui/ReviewScreen.h"
#include "ui/UiApp.h"
#include "ui/VaultScreen.h"

#include "client/AccountIds.h"
#include "core/Vault.h"

#include <SDL3/SDL_main.h>

#include <cstdlib>

using namespace admin;
using namespace admin::ui_host;
using namespace admin::online;

int main(int argc, char **argv) {
  std::vector<std::string> args;
  AppOptions options = UiApp::parseArgs(argc, argv, {.title = "Reserve — pp-ledger admin"}, args);
  UiApp app(options);
  if (!app.start()) {
    return 1;
  }
  Session session(Vault::defaultDir());
  Online online(app, session);
  Nav nav(session);
  VaultScreen vault(session);
  ReviewScreen review(session);
  KeysScreen keys(session);
  NetworkScreen network(online);
  TransferScreen transfer(online, pp::AccountIds::ID_RESERVE, "reserve", [&](const std::string &path) {
    review.open(path);
    session.navigate("review");
  });
  online.onStatus = [&](const std::string &status) { nav.setStatus(status); };
  if (!nav.build(app.context(), "Reserve",
                 {{"transfer", "New transfer"}, {"review", "Review and submit"}, {"keys", "Keys and people"},
                  {"network", "Network"}}) ||
      !vault.build(app) || !review.build(app) || !keys.build(app) || !network.build(app) || !transfer.build(app)) {
    return 1;
  }
  bool loaded = false;
  session.navigate = [&](const std::string &screen) {
    if (!session.vault.isUnlocked() || screen == "vault") {
      loaded = false;
      app.show(vault);
      return;
    }
    if (!loaded) { // just unlocked: the node it talks to is kept in the vault
      online.load();
      loaded = true;
    }
    nav.select(screen);
    if (screen == "keys") {
      app.show(keys);
    } else if (screen == "network") {
      app.show(network);
    } else if (screen == "review") {
      app.show(review);
    } else {
      nav.select("transfer");
      app.show(transfer);
    }
  };
  int idleSeconds = Session::kIdleLockMinutes * 60;
#ifdef PP_ADMIN_DEV_HOOKS
  if (const char *seconds = std::getenv("PP_ADMIN_DEV_IDLE_SECONDS")) {
    idleSeconds = std::atoi(seconds);
  }
#endif
  app.setIdleTimeout(idleSeconds, [&] {
    if (session.vault.isUnlocked()) {
      session.vault.lock();
      session.lockedBecause = "Locked after " +
                              (idleSeconds % 60 == 0 ? std::to_string(idleSeconds / 60) + " minutes"
                                                     : std::to_string(idleSeconds) + " seconds") +
                              " without use.";
      session.navigate("vault");
    }
  });
  if (!args.empty()) {
    review.open(args.front());
  }
  session.navigate("vault");
  return app.run();
}
