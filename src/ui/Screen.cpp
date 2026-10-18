#include "ui/Screen.h"

#include "ui/UiApp.h"

#include <ui/dom/Context.h>

namespace admin::ui_host {

bool Screen::build(UiApp &app) {
  app_ = &app;
  ui::DataModelConstructor constructor = app.context()->CreateDataModel(modelName());
  if (!constructor) {
    return false;
  }
  bind(constructor);
  model_ = constructor.GetModelHandle();
  document_ = app.loadView(viewName());
  return document_ != nullptr;
}

} // namespace admin::ui_host
