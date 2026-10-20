#include "online/OperationForm.h"

#include "core/Amounts.h"
#include "ledger/client/AccountIds.h"

#include <chrono>
#include <filesystem>
#include <memory>

namespace apps::online {

namespace op = pp::operation;

namespace {
const std::vector<ui_host::FileFilter> kOperations = {{"Operation files", "json"}};
} // namespace

int64_t nowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string accountTitle(uint64_t id) {
  switch (id) {
  case pp::AccountIds::ID_GENESIS:
    return "Genesis (admin)";
  case pp::AccountIds::ID_RESERVE:
    return "Reserve";
  case pp::AccountIds::ID_REGISTRAR:
    return "Registrar";
  case pp::AccountIds::ID_FEE:
    return "Fee";
  case pp::AccountIds::ID_RECYCLE:
    return "Recycle";
  default:
    return "account " + formatAmount(id);
  }
}

void OperationForm::bindForm(ui::DataModelConstructor &model) {
  model.Bind("valid_hours", &validHours_);
  model.Bind("notes", &notes_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
}

void OperationForm::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void OperationForm::make(std::function<pp::Roe<op::Operation>(net::ChainLink &, const net::ChainInfo &, int64_t,
                                                             const std::string &)>
                             build,
                         std::string suggested) {
  if (busy_) {
    return;
  }
  auto hours = parseAmount(validHours_);
  if (!hours || hours.value() == 0) {
    say("Type how many hours the signers have (24 is a day).");
    return;
  }
  busy_ = true;
  say("", "Making the operation…");
  auto made = std::make_shared<op::Operation>();
  const int64_t validFor = static_cast<int64_t>(hours.value()) * 3600;
  const std::string notes = notes_;
  online_.withChain(
      [build = std::move(build), made, validFor, notes](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        auto operation = build(link, chain.value(), validFor, notes);
        if (!operation) {
          return operation.error().message;
        }
        *made = std::move(operation.value());
        return "";
      },
      [this, made, suggested = std::move(suggested)](const std::string &error) {
        busy_ = false;
        if (!error.empty()) {
          say(error + ".");
          return;
        }
        app_->chooseFileToSave(kOperations, suggested, [this, made](std::optional<std::string> path) {
          if (!path) {
            say("", "Not saved.");
            return;
          }
          if (auto saved = op::save(*made, *path); !saved) {
            say(saved.error().message);
            return;
          }
          notes_.clear();
          clearForm();
          say("", "Saved " + std::filesystem::path(*path).filename().string() + ": hand a copy to each signer.");
          made_(*path);
        });
      });
}

} // namespace apps::online
