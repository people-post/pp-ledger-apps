#include "online/TransferScreen.h"

#include "core/Amounts.h"
#include "client/AccountIds.h"
#include "lib/common/BinaryPack.hpp"
#include "lib/common/Utilities.h"

#include <chrono>
#include <filesystem>
#include <memory>

namespace admin::online {

namespace op = pp::operation;

namespace {

const std::vector<ui_host::FileFilter> kOperations = {{"Operation files", "json"}};

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

} // namespace

void TransferScreen::bind(ui::DataModelConstructor &model) {
  model.Bind("from", &fromName_);
  model.Bind("balance", &balance_);
  model.Bind("signers", &signers_);
  model.Bind("to", &to_);
  model.Bind("amount", &amount_);
  model.Bind("fee", &fee_);
  model.Bind("valid_hours", &validHours_);
  model.Bind("notes", &notes_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
  model.BindEventCallback("transfer_make", &TransferScreen::make, this);
}

void TransferScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void TransferScreen::willShow() {
  balance_ = "…";
  signers_ = "…";
  say("");
  auto balance = std::make_shared<std::string>();
  auto signers = std::make_shared<std::string>();
  auto fee = std::make_shared<std::string>();
  const uint64_t from = from_;
  online_.withChain(
      [from, balance, signers, fee](net::ChainLink &link) -> std::string {
        auto account = link.account(from);
        if (!account) {
          return account.error().message;
        }
        const auto &wallet = account.value().wallet;
        const auto native = wallet.mBalances.find(pp::AccountIds::ID_GENESIS);
        *balance = formatAmount(native == wallet.mBalances.end() || native->second < 0
                                    ? 0
                                    : static_cast<uint64_t>(native->second));
        *signers = std::to_string(wallet.minSignatures) + " of " + std::to_string(wallet.publicKeys.size());
        // A plain transfer's least fee (no metadata): what the form starts from.
        pp::Ledger::TxDefault probe;
        probe.fromWalletId = from;
        pp::Ledger::Record record;
        record.type = pp::Ledger::T_DEFAULT;
        record.data = pp::utl::binaryPack(probe);
        if (auto least = link.minimumFee(record)) {
          *fee = formatAmount(least.value());
        }
        return "";
      },
      [this, balance, signers, fee](const std::string &error) {
        balance_ = error.empty() ? *balance : "unknown";
        signers_ = error.empty() ? *signers : "unknown";
        if (fee_.empty()) {
          fee_ = *fee;
        }
        say(error.empty() ? "" : error + ".");
      });
}

void TransferScreen::make(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (busy_) {
    return;
  }
  auto to = parseAmount(to_);
  auto amount = parseAmount(amount_);
  auto fee = parseAmount(fee_);
  auto hours = parseAmount(validHours_);
  if (!to) {
    say("Type the account it goes to (its number).");
    return;
  }
  if (to.value() == from_) {
    say("It cannot go to the account it comes from.");
    return;
  }
  if (!amount || amount.value() == 0) {
    say(amount ? "Type an amount above 0." : amount.error().message + ".");
    return;
  }
  if (!fee) {
    say("Type the fee: " + fee.error().message + ".");
    return;
  }
  if (!hours || hours.value() == 0) {
    say("Type how many hours the signers have (24 is a day).");
    return;
  }
  busy_ = true;
  say("", "Making the operation…");
  auto made = std::make_shared<op::Operation>();
  const uint64_t from = from_;
  const uint64_t toId = to.value();
  const uint64_t amountValue = amount.value();
  const uint64_t feeValue = fee.value();
  const int64_t validFor = static_cast<int64_t>(hours.value()) * 3600;
  const std::string notes = notes_;
  online_.withChain(
      [=](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        auto signers = link.signersOf(from);
        if (!signers) {
          return signers.error().message;
        }
        const auto longest = static_cast<int64_t>(chain.value().meta.config.maxValidationTimespanSeconds);
        if (longest > 0 && validFor > longest) {
          return "The chain lets an operation wait at most " + std::to_string(longest / 3600) + " hours";
        }
        op::Common common;
        common.networkId = chain.value().networkId;
        common.genesisHash = chain.value().genesisHash;
        common.signers = signers.value();
        common.window = op::windowFrom(nowSeconds(), validFor);
        common.notes = notes;
        common.createdAt = nowSeconds();
        auto transfer = op::transfer(common, toId, amountValue, feeValue);
        if (!transfer) {
          return transfer.error().message;
        }
        auto least = link.minimumFee(transfer.value().record);
        if (least && feeValue < least.value()) {
          return "The fee must be at least " + formatAmount(least.value());
        }
        *made = std::move(transfer.value());
        return "";
      },
      [this, made, toId, amountValue](const std::string &error) {
        busy_ = false;
        if (!error.empty()) {
          say(error + ".");
          return;
        }
        const std::string suggested =
            "transfer-" + formatAmount(amountValue) + "-to-" + std::to_string(toId) + ".json";
        app_->chooseFileToSave(kOperations, suggested, [this, made, toId](std::optional<std::string> path) {
          if (!path) {
            say("", "Not saved.");
            return;
          }
          if (auto saved = op::save(*made, *path); !saved) {
            say(saved.error().message);
            return;
          }
          to_.clear();
          amount_.clear();
          notes_.clear();
          say("", "Saved " + std::filesystem::path(*path).filename().string() + " (to " + accountTitle(toId) +
                      "): hand a copy to each signer.");
          made_(*path);
        });
      });
}

} // namespace admin::online
