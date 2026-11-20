#include "ui/Nav.h"

#include <ui/dom/Context.h>

namespace apps::ui_host {

bool Nav::build(ui::Context *context, const std::string &appTitle, std::vector<Tab> tabs) {
  appTitle_ = appTitle;
  tabs_ = std::move(tabs);
  ui::DataModelConstructor model = context->CreateDataModel("nav");
  if (!model) {
    return false;
  }
  if (auto tab = model.RegisterStruct<Tab>()) {
    tab.RegisterMember("id", &Tab::id);
    tab.RegisterMember("title", &Tab::title);
  }
  model.RegisterArray<std::vector<Tab>>();
  model.Bind("app", &appTitle_);
  model.Bind("tabs", &tabs_);
  model.Bind("current", &current_);
  model.Bind("status", &status_);
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

void Nav::setStatus(const std::string &status) {
  status_ = status;
  model_.DirtyAllVariables();
}

} // namespace apps::ui_host
