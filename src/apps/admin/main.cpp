// pp-admin-admin: for the holders of the admin (genesis account) keys. Mint to the reserve, change
// the config, rotate the admin set, certify a new beacon key; sign, collect, submit.
// Usage: pp-admin-admin [operation file]
#include "online/AdminScreens.h"
#include "online/NetworkScreen.h"
#include "online/Online.h"
#include "ui/IdleLock.h"
#include "ui/KeysScreen.h"
#include "ui/Nav.h"
#include "ui/ReviewScreen.h"
#include "ui/UiApp.h"
#include "ui/VaultScreen.h"

#include "core/Vault.h"

#include <SDL3/SDL_main.h>

#include <map>

using namespace admin;
using namespace admin::ui_host;
using namespace admin::online;

int main(int argc, char **argv) {
  std::vector<std::string> args;
  AppOptions options = UiApp::parseArgs(argc, argv, {.title = "Admin — pp-ledger admin", .width = 1200}, args);
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
  auto made = [&](const std::string &path) {
    review.open(path);
    session.navigate("review");
  };
  MintScreen mint(online, made);
  ConfigScreen config(online, made);
  AdminKeysScreen admins(online, session, made);
  BeaconScreen beacon(online, session, made);
  online.onStatus = [&](const std::string &status) { nav.setStatus(status); };
  const std::map<std::string, Screen *> screens = {{"mint", &mint},     {"config", &config},  {"admins", &admins},
                                                   {"beacon", &beacon}, {"review", &review},  {"keys", &keys},
                                                   {"network", &network}};
  if (!nav.build(app.context(), "Admin",
                 {{"mint", "Mint"},
                  {"config", "Config"},
                  {"admins", "Admin set"},
                  {"beacon", "Beacon"},
                  {"review", "Review"},
                  {"keys", "Keys"},
                  {"network", "Network"}}) ||
      !vault.build(app)) {
    return 1;
  }
  for (const auto &[name, screen] : screens) {
    if (!screen->build(app)) {
      return 1;
    }
  }
  bool loaded = false;
  session.navigate = [&](const std::string &name) {
    if (!session.vault.isUnlocked() || name == "vault") {
      loaded = false;
      app.show(vault);
      return;
    }
    if (!loaded) { // just unlocked: the node it talks to is kept in the vault
      online.load();
      loaded = true;
    }
    const auto it = screens.find(name);
    const std::string shown = it == screens.end() ? "review" : name;
    nav.select(shown);
    app.show(*screens.at(shown));
  };
  installIdleLock(app, session);
  if (!args.empty()) {
    review.open(args.front());
  }
  session.navigate("vault");
  return app.run();
}
