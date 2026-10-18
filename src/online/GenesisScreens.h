#pragma once
// The Genesis app's forms (DESIGN.md § The genesis ceremony): the genesis plan — the network, its
// start, who holds each system account and the recovery set (from people's key cards), the beacon's
// key, the genesis miners, the settings — and later, a recovery: the recovery set naming a new admin
// set after the admin keys are lost or stolen.

#include "online/OperationForm.h"
#include "ui/Screen.h"
#include "ui/Session.h"

#include <functional>
#include <vector>

namespace admin::online {

/** A key offered for a place in the plan: from a key card or the vault. */
struct KeyChoice {
  std::string name;
  std::string fingerprint;
  bool chosen{false};
};

class GenesisPlanScreen : public ui_host::Screen {
public:
  GenesisPlanScreen(ui_host::Session &session, std::function<void(const std::string &)> made)
      : session_(session), made_(std::move(made)) {}
  void willShow() override;

  struct Section {
    std::string role;
    std::string title;
    std::string help;
    std::string min;
    std::vector<KeyChoice> keys;
  };
  struct Miner {
    std::string name;
    std::string fingerprint;
    bool chosen{true};
    std::string stake;
  };
  struct Setting {
    std::string name;
    std::string title;
    std::string value;
  };

protected:
  std::string modelName() const override { return "plan"; }
  std::string viewName() const override { return "plan.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void makePlan(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void say(const std::string &error, const std::string &notice = "");
  /** The public keys behind `section`'s choices, chosen ones. */
  std::vector<std::string> chosenKeys(size_t section) const;

  ui_host::Session &session_;
  std::function<void(const std::string &)> made_;
  std::string networkId_;
  std::string start_;
  std::vector<Section> sections_;
  std::vector<std::vector<std::string>> sectionKeys_;
  std::vector<KeyChoice> beacons_;
  std::vector<std::string> beaconKeys_;
  std::vector<Miner> miners_;
  std::vector<std::string> minerKeys_;
  std::vector<Setting> settings_;
  std::string recoveryDelay_;
  std::string notes_;
  std::string error_;
  std::string notice_;
  bool filled_{false};
};

class RecoveryScreen : public OperationForm {
public:
  RecoveryScreen(Online &online, ui_host::Session &session, std::function<void(const std::string &)> made)
      : OperationForm(online, std::move(made)), session_(session) {}
  void willShow() override;

protected:
  std::string modelName() const override { return "recovery"; }
  std::string viewName() const override { return "recovery.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void makeRecovery(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  ui_host::Session &session_;
  std::string recovery_;
  std::string admins_;
  std::vector<KeyChoice> candidates_;
  std::vector<std::string> candidateKeys_;
  std::string threshold_{"2"};
};

} // namespace admin::online
