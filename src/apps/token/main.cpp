// pp-ledger-token: for a token issuer's operators (projects/token-app). The issuer's overview, issuing
// its token, sending from it, replacing its signers; sign, collect, submit.
// Usage: pp-ledger-token [operation file]
#include "online/NetworkScreen.h"
#include "online/Online.h"
#include "online/TokenScreens.h"
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
  AppOptions options = UiApp::parseArgs(argc, argv, {.title = "Token — PP Ledger"}, args);
  UiApp app(options);
  if (!app.start()) {
    return 1;
  }
  Session session(Vault::defaultDir());
  Online online(app, session);
  Issuer issuer(session);
  Nav nav(session);
  VaultScreen vault(session);
  ReviewScreen review(session);
  KeysScreen keys(session, "owner");
  NetworkScreen network(online);
  auto made = [&](const std::string &path) {
    review.open(path);
    session.navigate("review");
  };
  TokenOverviewScreen overview(online, session, issuer);
  IssueScreen issue(online, issuer, made);
  SendScreen send(online, issuer, made);
  SignersScreen signers(online, session, issuer, made);
  online.onStatus = [&](const std::string &status) { nav.setStatus(status); };
  const std::map<std::string, Screen *> screens = {{"overview", &overview}, {"issue", &issue},   {"send", &send},
                                                   {"signers", &signers},   {"review", &review}, {"keys", &keys},
                                                   {"network", &network}};
  if (!nav.build(app.context(), "Token",
                 {{"overview", "Overview"},
                  {"issue", "Issue"},
                  {"send", "Send"},
                  {"signers", "Signers"},
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
    if (!loaded) { // just unlocked: the node and the issuer are kept in the vault
      online.load();
      issuer.load();
      loaded = true;
    }
    const auto it = screens.find(name);
    const std::string shown = it == screens.end() ? "overview" : name;
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
