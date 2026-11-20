// pp-ledger-reserve: for the reserve's holders. Make a transfer out of the treasury, sign it, collect
// the others' signatures, submit it and see it land. Usage: pp-ledger-reserve [operation file]
#include "online/NetworkScreen.h"
#include "online/Online.h"
#include "online/TransferScreen.h"
#include "ui/IdleLock.h"
#include "ui/KeysScreen.h"
#include "ui/Nav.h"
#include "ui/ReviewScreen.h"
#include "ui/UiApp.h"
#include "ui/VaultScreen.h"

#include "ledger/client/AccountIds.h"
#include "core/Vault.h"

#include <SDL3/SDL_main.h>

#include <cstdlib>

using namespace apps;
using namespace apps::ui_host;
using namespace apps::online;

int main(int argc, char **argv) {
  std::vector<std::string> args;
  AppOptions options = UiApp::parseArgs(argc, argv, {.title = "Reserve — PP Ledger"}, args);
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
  installIdleLock(app, session);
  if (!args.empty()) {
    review.open(args.front());
  }
  session.navigate("vault");
  return app.run();
}
