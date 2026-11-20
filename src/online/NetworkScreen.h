#pragma once
// The node a role app talks to: its address (from the node's operator), checked by asking it what
// chain it serves.

#include "online/Online.h"
#include "ui/Screen.h"

#include <vector>

namespace apps::online {

class NetworkScreen : public ui_host::Screen {
public:
  NetworkScreen(Online &online) : online_(online) {}
  void willShow() override;

  struct Fact {
    std::string label;
    std::string value;
  };

protected:
  std::string modelName() const override { return "network"; }
  std::string viewName() const override { return "network.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void check(ui::DataModelHandle, ui::Event &, const ui::VariantList &);

  Online &online_;
  std::string address_;
  std::vector<Fact> facts_;
  std::string error_;
  std::string notice_;
  bool busy_{false};
};

} // namespace apps::online
