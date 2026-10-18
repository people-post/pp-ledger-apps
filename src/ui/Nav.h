#pragma once
// The bar at the top of an unlocked app: its screens as tabs, a status (the network a role app is
// linked to), and locking the vault. A data model ("nav") every view's header binds.

#include "ui/Session.h"

#include <ui/data/DataModelHandle.h>

#include <string>
#include <vector>

namespace ui {
class Context;
}

namespace admin::ui_host {

class Nav {
public:
  struct Tab {
    std::string id;
    std::string title;
  };

  explicit Nav(Session &session) : session_(session) {}
  bool build(ui::Context *context, const std::string &appTitle, std::vector<Tab> tabs);
  /** Mark `screen` as the one shown. */
  void select(const std::string &screen);
  /** A short status at the right of the bar ("" for none). */
  void setStatus(const std::string &status);

private:
  Session &session_;
  std::string appTitle_;
  std::vector<Tab> tabs_;
  std::string current_;
  std::string status_;
  ui::DataModelHandle model_;
};

} // namespace admin::ui_host
