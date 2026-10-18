// pp-ledger-wallet: everyone's last-resort wallet (projects/wallet-app). Accounts proven from the
// network's pinned genesis hash, believing no node; review and sign operations.
// Usage: pp-ledger-wallet [operation file]
#include "online/Online.h"
#include "online/WalletScreens.h"
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
  AppOptions options = UiApp::parseArgs(argc, argv, {.title = "Wallet — PP Ledger"}, args);
  UiApp app(options);
  if (!app.start()) {
    return 1;
  }
  Session session(Vault::defaultDir());
  Online online(app, session);
  WalletDesk desk(online, session);
  Nav nav(session);
  VaultScreen vault(session);
  ReviewScreen review(session);
  KeysScreen keys(session, "owner");
  WalletNetworkScreen network(online, desk);
  AccountsScreen accounts(online, session, desk);
  auto made = [&](const std::string &path) {
    review.open(path);
    session.navigate("review");
  };
  WalletSendScreen send(online, session, desk, made);
  EvacuateScreen evacuate(online, session, desk, made);
  online.onStatus = [&](const std::string &status) { nav.setStatus(status); };
  const std::map<std::string, Screen *> screens = {{"accounts", &accounts}, {"send", &send},
                                                   {"evacuate", &evacuate}, {"review", &review},
                                                   {"keys", &keys},         {"network", &network}};
  if (!nav.build(app.context(), "Wallet",
                 {{"accounts", "Accounts"},
                  {"send", "Send"},
                  {"evacuate", "Evacuate"},
                  {"review", "Review and sign"},
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
    if (!loaded) { // just unlocked: the network pinned, the accounts followed, the node asked
      online.load();
      if (const std::string error = desk.load(); !error.empty()) {
        nav.setStatus(error);
      }
      loaded = true;
    }
    const auto it = screens.find(name);
    // No network pinned yet: that comes first.
    const std::string shown = it == screens.end() ? (desk.book().card ? "accounts" : "network") : name;
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
