#pragma once
// What a role app adds to the shared screens: the node it talks to (its address kept in the vault),
// chain work off the UI thread one call at a time, and sending operations then following them into
// a block (Session::submit).

#include "net/ChainLink.h"
#include "ui/Session.h"
#include "ui/UiApp.h"

#include <functional>
#include <mutex>
#include <string>

namespace admin::online {

class Online {
public:
  Online(ui_host::UiApp &app, ui_host::Session &session);

  /** The vault file the node address is kept in. */
  static constexpr const char *kSettingsFile = "network.json";

  /** Read the node address from the vault (after it unlocks). */
  void load();
  const std::string &address() const { return address_; }
  pp::Roe<void> saveAddress(const std::string &address);

  /**
   * Run `work` with the link to the node (linked first when needed) off the UI thread; `done` gets
   * its error ("" when none) on the UI thread.
   */
  void withChain(std::function<std::string(net::ChainLink &)> work, std::function<void(const std::string &)> done);

  /** Session::submit: send, then look for it in a block every few seconds until its window ends. */
  void submit(const pp::operation::Operation &operation,
              std::function<void(const ui_host::Session::SubmitProgress &)> report);

  /** Called on the UI thread with a short status for the bar ("pp-mainnet", "No node set", …). */
  std::function<void(const std::string &)> onStatus;

private:
  /** Link to the address set, if not linked to it yet; "" or the error. Holds `mutex_`. */
  std::string linkLocked();

  ui_host::UiApp &app_;
  ui_host::Session &session_;
  std::mutex mutex_;
  net::ChainLink link_;
  std::string address_;
};

} // namespace admin::online
