#pragma once
// The bar at the top of an unlocked app: its screens, and locking the vault. A data model ("nav")
// every view's header binds.

#include "ui/Session.h"

#include <ui/data/DataModelHandle.h>

#include <string>

namespace ui {
class Context;
}

namespace admin::ui_host {

class Nav {
public:
  explicit Nav(Session &session) : session_(session) {}
  bool build(ui::Context *context, const std::string &appTitle);
  /** Mark `screen` as the one shown. */
  void select(const std::string &screen);

private:
  Session &session_;
  std::string appTitle_;
  std::string current_;
  ui::DataModelHandle model_;
};

} // namespace admin::ui_host
