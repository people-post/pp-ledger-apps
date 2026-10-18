#pragma once
// A form that makes an operation from the chain (a transfer, a mint, a config update, …): what every
// such form shares — how long the signers have, notes for them, building against the chain off the
// UI thread, saving the file, opening it for review.

#include "net/ChainLink.h"
#include "online/Online.h"
#include "ui/Screen.h"

#include <functional>
#include <string>

namespace admin::online {

class OperationForm : public ui_host::Screen {
public:
  OperationForm(Online &online, std::function<void(const std::string &)> made)
      : online_(online), made_(std::move(made)) {}

protected:
  /** Bind the shared fields: valid_hours, notes, error, notice, busy. */
  void bindForm(ui::DataModelConstructor &model);
  void say(const std::string &error, const std::string &notice = "");

  /**
   * Build an operation against the chain (`build`, off the UI thread) with the form's hours and
   * notes, then ask where to save it (`suggested` name) and hand it on for review.
   */
  void make(std::function<pp::Roe<pp::operation::Operation>(net::ChainLink &, const net::ChainInfo &, int64_t validFor,
                                                           const std::string &notes)>
                build,
            std::string suggested);
  /** After the file is saved: clear the form's own fields. */
  virtual void clearForm() {}

  Online &online_;
  std::string error_;
  std::string notice_;
  bool busy_{false};

private:
  std::function<void(const std::string &)> made_;
  std::string validHours_{"24"};
  std::string notes_;
};

/** "Reserve", "Registrar", …, or "account 1,048,576". */
std::string accountTitle(uint64_t id);
int64_t nowSeconds();

} // namespace admin::online
