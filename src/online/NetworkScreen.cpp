#include "online/NetworkScreen.h"

#include "core/Amounts.h"
#include "lib/common/Utilities.h"

#include <algorithm>
#include <cctype>
#include <memory>

namespace apps::online {

void NetworkScreen::bind(ui::DataModelConstructor &model) {
  if (auto fact = model.RegisterStruct<Fact>()) {
    fact.RegisterMember("label", &Fact::label);
    fact.RegisterMember("value", &Fact::value);
  }
  model.RegisterArray<std::vector<Fact>>();
  model.Bind("address", &address_);
  model.Bind("facts", &facts_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
  model.BindEventCallback("network_check", &NetworkScreen::check, this);
}

void NetworkScreen::willShow() {
  address_ = online_.address();
  error_.clear();
  notice_.clear();
  refresh();
}

void NetworkScreen::check(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (busy_) {
    return;
  }
  std::string address = address_;
  address.erase(std::remove_if(address.begin(), address.end(), [](unsigned char c) { return std::isspace(c); }),
                address.end());
  // Saved first: Online links to the address set.
  if (auto saved = online_.saveAddress(address); !saved) {
    error_ = saved.error().message;
    refresh();
    return;
  }
  busy_ = true;
  error_.clear();
  notice_ = "Asking the node…";
  facts_.clear();
  refresh();
  auto facts = std::make_shared<std::vector<Fact>>();
  auto network = std::make_shared<std::string>();
  online_.withChain(
      [facts, network](net::ChainLink &link) -> std::string {
        auto info = link.info();
        if (!info) {
          return info.error().message;
        }
        const auto &chain = info.value();
        *network = chain.networkId;
        const auto &admins = chain.meta.genesis.wallet;
        facts->push_back({"Network", chain.networkId});
        facts->push_back({"Blocks", formatAmount(chain.nextBlockId)});
        facts->push_back({"Admin keys", std::to_string(admins.minSignatures) + " of " +
                                            std::to_string(admins.publicKeys.size()) + " sign"});
        facts->push_back({"Longest validity (hours)",
                          std::to_string(chain.meta.config.maxValidationTimespanSeconds / 3600)});
        if (chain.meta.hasPending) {
          facts->push_back({"Config update pending", "from epoch " + formatAmount(chain.meta.pending.activationEpoch)});
        }
        return "";
      },
      [this, facts, network](const std::string &error) {
        busy_ = false;
        facts_ = *facts;
        error_ = error.empty() ? "" : error + ".";
        notice_ = error.empty() ? "The node answers. Saved." : "";
        if (online_.onStatus) {
          online_.onStatus(error.empty() ? *network : "Node not answering");
        }
        refresh();
      });
}

} // namespace apps::online
