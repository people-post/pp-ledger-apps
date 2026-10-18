#include "ui/Nav.h"

#include <ui/dom/Context.h>

namespace admin::ui_host {

bool Nav::build(ui::Context *context, const std::string &appTitle) {
  appTitle_ = appTitle;
  ui::DataModelConstructor model = context->CreateDataModel("nav");
  if (!model) {
    return false;
  }
  model.Bind("app", &appTitle_);
  model.Bind("current", &current_);
  model.BindEventCallback("nav_go", [this](ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
    if (!args.empty()) {
      session_.navigate(args[0].Get<ui::String>());
    }
  });
  model.BindEventCallback("nav_lock", [this](ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
    session_.vault.lock();
    session_.navigate("vault");
  });
  model_ = model.GetModelHandle();
  return true;
}

void Nav::select(const std::string &screen) {
  current_ = screen;
  model_.DirtyAllVariables();
}

} // namespace admin::ui_host
