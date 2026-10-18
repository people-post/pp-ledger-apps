#pragma once
// A transfer from the account the app operates (the reserve, …): a form made into an operation
// file for its holders to sign (DESIGN.md § How an operation travels).

#include "online/OperationForm.h"

namespace admin::online {

class TransferScreen : public OperationForm {
public:
  /** Transfers from account `from` ("reserve"); `made` is told of each operation file written. */
  TransferScreen(Online &online, uint64_t from, std::string fromName, std::function<void(const std::string &)> made)
      : OperationForm(online, std::move(made)), from_(from), fromName_(std::move(fromName)) {}
  void willShow() override;

protected:
  std::string modelName() const override { return "transfer"; }
  std::string viewName() const override { return "transfer.rml"; }
  void bind(ui::DataModelConstructor &model) override;
  void clearForm() override;

private:
  void makeTransfer(ui::DataModelHandle, ui::Event &, const ui::VariantList &);

  uint64_t from_;
  std::string fromName_;
  std::string balance_;
  std::string signers_;
  std::string to_;
  std::string amount_;
  std::string fee_;
};

} // namespace admin::online
