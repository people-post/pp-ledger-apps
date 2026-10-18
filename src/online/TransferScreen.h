#pragma once
// A transfer from the account the app operates (the reserve, …): a form made into an operation
// file for its holders to sign (DESIGN.md § How an operation travels).

#include "online/Online.h"
#include "ui/Screen.h"

#include <functional>

namespace admin::online {

class TransferScreen : public ui_host::Screen {
public:
  /** Transfers from account `from` ("Reserve"); `made` is told of each operation file written. */
  TransferScreen(Online &online, uint64_t from, std::string fromName, std::function<void(const std::string &)> made)
      : online_(online), from_(from), fromName_(std::move(fromName)), made_(std::move(made)) {}
  void willShow() override;

protected:
  std::string modelName() const override { return "transfer"; }
  std::string viewName() const override { return "transfer.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void make(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void say(const std::string &error, const std::string &notice = "");

  Online &online_;
  uint64_t from_;
  std::string fromName_;
  std::function<void(const std::string &)> made_;

  std::string balance_;
  std::string signers_;
  std::string to_;
  std::string amount_;
  std::string fee_;
  std::string validHours_{"24"};
  std::string notes_;
  std::string error_;
  std::string notice_;
  bool busy_{false};
};

} // namespace admin::online
