#pragma once
// The keys in the vault and the people in the address book: make or import a key, hand out its key
// card, add others' cards, back the vault up or restore it (DESIGN.md § Vault, § The genesis
// ceremony).

#include "ui/Screen.h"
#include "ui/Session.h"

#include <vector>

namespace admin::ui_host {

class KeysScreen : public Screen {
public:
  explicit KeysScreen(Session &session) : session_(session) {}
  void willShow() override;

  struct KeyRow {
    std::string id;
    std::string label;
    std::string role;
    std::string fingerprint;
  };
  struct CardRow {
    std::string name;
    std::string role;
    std::string fingerprint;
  };

protected:
  std::string modelName() const override { return "keys"; }
  std::string viewName() const override { return "keys.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void generate(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void importKey(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void exportCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void addCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void removeCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void backup(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void restore(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void reload();
  void say(const std::string &error, const std::string &notice = "");

  Session &session_;
  std::vector<KeyRow> keys_;
  std::vector<CardRow> cards_;
  std::vector<std::string> cardKeys_;
  std::string newLabel_;
  std::string newRole_{"admin"};
  std::string backupPassphrase_;
  std::string error_;
  std::string notice_;
  bool busy_{false};
};

} // namespace admin::ui_host
