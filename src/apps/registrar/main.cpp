// pp-ledger-registrar: for the registrar's holders. Create issued accounts for their owners and fund
// them; sign, collect, submit.
// Usage: pp-ledger-registrar [operation file]
#include "online/RegistrarScreen.h"
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

using namespace apps;
using namespace apps::ui_host;
using namespace apps::online;

int main(int argc, char **argv) {
  std::vector<std::string> args;
  AppOptions options = UiApp::parseArgs(argc, argv, {.title = "Registrar — PP Ledger"}, args);
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
  RegistrarScreen account(online, made);
  online.onStatus = [&](const std::string &status) { nav.setStatus(status); };
  const std::map<std::string, Screen *> screens = {
      {"account", &account}, {"review", &review}, {"keys", &keys}, {"network", &network}};
  if (!nav.build(app.context(), "Registrar",
                 {{"account", "New account"}, {"review", "Review and submit"}, {"keys", "Keys and people"},
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
    const std::string shown = it == screens.end() ? "account" : name;
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
