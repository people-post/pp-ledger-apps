#pragma once
// The Admin app's forms (DESIGN.md § Who uses them): mint to the reserve, change the config, rotate
// the admin set, certify the next beacon key. Each builds against the chain (net/AdminOperations).

#include "online/OperationForm.h"

#include "core/KeyCard.h"

#include <vector>

namespace apps::online {

class MintScreen : public OperationForm {
public:
  using OperationForm::OperationForm;
  void willShow() override;

protected:
  std::string modelName() const override { return "mint"; }
  std::string viewName() const override { return "mint.rml"; }
  void bind(ui::DataModelConstructor &model) override;
  void clearForm() override { amount_.clear(); }

private:
  void makeMint(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  std::string cap_;
  std::string reserve_;
  std::string amount_;
  std::string fee_;
};

class ConfigScreen : public OperationForm {
public:
  using OperationForm::OperationForm;
  void willShow() override;

  struct Row {
    std::string name;
    std::string title;
    std::string current;
    std::string next;
  };

protected:
  std::string modelName() const override { return "config"; }
  std::string viewName() const override { return "config.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void makeUpdate(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  std::vector<Row> rows_;
  std::string pending_;
};

class AdminKeysScreen : public OperationForm {
public:
  AdminKeysScreen(Online &online, ui_host::Session &session, std::function<void(const std::string &)> made)
      : OperationForm(online, std::move(made)), session_(session) {}
  void willShow() override;

  struct Candidate {
    std::string name;
    std::string fingerprint;
    bool chosen{false};
  };

protected:
  std::string modelName() const override { return "admins"; }
  std::string viewName() const override { return "admins.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void makeRotation(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  ui_host::Session &session_;
  std::string current_;
  std::vector<Candidate> candidates_;
  std::vector<std::string> candidateKeys_;
  std::string threshold_{"2"};
};

class BeaconScreen : public OperationForm {
public:
  BeaconScreen(Online &online, ui_host::Session &session, std::function<void(const std::string &)> made)
      : OperationForm(online, std::move(made)), session_(session) {}
  void willShow() override;

protected:
  std::string modelName() const override { return "beacon"; }
  std::string viewName() const override { return "beacon.rml"; }
  void bind(ui::DataModelConstructor &model) override;
  void clearForm() override;

private:
  void chooseCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void makeCertificate(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  ui_host::Session &session_;
  std::string current_;
  std::string next_;
  std::string chosen_;
  std::string chosenKey_;
  std::string hints_;
};

} // namespace apps::online
