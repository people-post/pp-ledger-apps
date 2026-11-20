// pp-ledger-fees: for the fee account's holders (projects/fee-app). Rounds: a period read from the
// chain, the relays' measures, the rules, the payouts as one operation; sign, collect, submit.
// Usage: pp-ledger-fees [operation file]
#include "online/FeeScreens.h"
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
  AppOptions options = UiApp::parseArgs(argc, argv, {.title = "Fees — PP Ledger", .width = 1200}, args);
  UiApp app(options);
  if (!app.start()) {
    return 1;
  }
  Session session(Vault::defaultDir());
  Online online(app, session);
  FeeDesk desk(session);
  // Payouts are checked against the round they name; anything else as the other apps do.
  session.compare = [&](const pp::operation::Operation &operation,
                        std::function<void(std::vector<std::pair<std::string, std::string>>)> done) {
    compareWithRounds(online, desk, operation, std::move(done));
  };
  Nav nav(session);
  VaultScreen vault(session);
  ReviewScreen review(session);
  KeysScreen keys(session, "fee");
  NetworkScreen network(online);
  auto made = [&](const std::string &path) {
    review.open(path);
    session.navigate("review");
  };
  RoundsScreen rounds(online, session, desk);
  PeriodScreen period(online, desk);
  RelaysScreen relays(desk);
  RulesScreen rules(online, desk);
  PayoutsScreen payouts(online, desk, made);
  online.onStatus = [&](const std::string &status) { nav.setStatus(status); };
  const std::map<std::string, Screen *> screens = {
      {"rounds", &rounds}, {"period", &period}, {"relays", &relays}, {"rules", &rules},
      {"payouts", &payouts}, {"review", &review}, {"keys", &keys},     {"network", &network}};
  if (!nav.build(app.context(), "Fees",
                 {{"rounds", "Rounds"},
                  {"period", "Period"},
                  {"relays", "Relays"},
                  {"rules", "Rules"},
                  {"payouts", "Payouts"},
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
    if (!loaded) { // just unlocked: the node and the fee records are kept in the vault
      online.load();
      if (const std::string error = desk.load(); !error.empty()) {
        nav.setStatus(error);
      }
      loaded = true;
    }
    const auto it = screens.find(name);
    const std::string shown = it == screens.end() ? "rounds" : name;
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
