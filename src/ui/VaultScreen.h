#pragma once
// Create the vault, or unlock it: the first screen of every app (DESIGN.md § Vault).

#include "ui/Screen.h"
#include "ui/Session.h"

namespace admin::ui_host {

class VaultScreen : public Screen {
public:
  explicit VaultScreen(Session &session) : session_(session) {}
  void willShow() override;

protected:
  std::string modelName() const override { return "vault"; }
  std::string viewName() const override { return "vault.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void unlock(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void create(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void finish(const std::string &error);

  Session &session_;
  bool exists_{false};
  std::string passphrase_;
  std::string confirm_;
  std::string error_;
  bool busy_{false};
  std::string dir_;
};

} // namespace admin::ui_host
