#pragma once
// A new issued account (DESIGN.md § Who uses them: the Registrar app): its number, its owner's keys
// (from their key card or a pp-client .pub file), what it starts with — made into an operation the
// registrar's holders sign.

#include "online/OperationForm.h"

#include <vector>

namespace admin::online {

class RegistrarScreen : public OperationForm {
public:
  RegistrarScreen(Online &online, std::function<void(const std::string &)> made)
      : OperationForm(online, std::move(made)) {}
  void willShow() override;

  struct OwnerKey {
    std::string name;
    std::string fingerprint;
  };

protected:
  std::string modelName() const override { return "registrar"; }
  std::string viewName() const override { return "registrar.rml"; }
  void bind(ui::DataModelConstructor &model) override;
  void clearForm() override;

private:
  void pickId(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void addCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void addPub(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void removeKey(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void makeAccount(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void addKey(const std::string &publicKey, const std::string &name);
  /** The least fee for the keys listed, from the chain. */
  void updateFee();

  std::string balance_;
  std::string signers_;
  std::string id_;
  std::vector<OwnerKey> keys_;
  std::vector<std::string> keyBytes_;
  std::string minSignatures_{"1"};
  std::string amount_;
  std::string fee_;
};

} // namespace admin::online
