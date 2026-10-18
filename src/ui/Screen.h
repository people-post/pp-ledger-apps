#pragma once
// A screen: one RML view and the data model it binds. Shown by UiApp, one at a time.

#include <ui/data/DataModelHandle.h>

#include <string>

namespace ui {
class Context;
class ElementDocument;
} // namespace ui

namespace admin::ui_host {

class UiApp;

class Screen {
public:
  virtual ~Screen() = default;

  /** Register the data model and load the view (hidden); false when either fails. */
  bool build(UiApp &app);
  /** Before the screen is shown: refresh what it shows. */
  virtual void willShow() {}

  ui::ElementDocument *document() const { return document_; }

protected:
  /** The model name the view's data-model attribute uses. */
  virtual std::string modelName() const = 0;
  /** The view under assets/views. */
  virtual std::string viewName() const = 0;
  /** Bind the model's variables and events. */
  virtual void bind(ui::DataModelConstructor &model) = 0;
  /** Mark the model's variables changed. */
  void refresh() { model_.DirtyAllVariables(); }

  UiApp *app_{nullptr};

private:
  ui::ElementDocument *document_{nullptr};
  ui::DataModelHandle model_;
};

} // namespace admin::ui_host
