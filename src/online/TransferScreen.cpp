#include "online/TransferScreen.h"

#include "core/Amounts.h"
#include "ledger/client/AccountIds.h"
#include "ledger/lib/common/BinaryPack.hpp"
#include "ledger/lib/common/Utilities.h"

#include <memory>

namespace apps::online {

namespace op = pp::operation;

void TransferScreen::bind(ui::DataModelConstructor &model) {
  bindForm(model);
  model.Bind("from", &fromName_);
  model.Bind("balance", &balance_);
  model.Bind("signers", &signers_);
  model.Bind("to", &to_);
  model.Bind("amount", &amount_);
  model.Bind("fee", &fee_);
  model.BindEventCallback("transfer_make", &TransferScreen::makeTransfer, this);
}

void TransferScreen::clearForm() {
  to_.clear();
  amount_.clear();
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

void TransferScreen::makeTransfer(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  auto to = parseAmount(to_);
  auto amount = parseAmount(amount_);
  auto fee = parseAmount(fee_);
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
  const uint64_t from = from_;
  const uint64_t toId = to.value();
  const uint64_t amountValue = amount.value();
  const uint64_t feeValue = fee.value();
  make(
      [=](net::ChainLink &link, const net::ChainInfo &chain, int64_t validFor,
          const std::string &notes) -> pp::Roe<op::Operation> {
        auto signers = link.signersOf(from);
        if (!signers) {
          return signers.error();
        }
        const auto longest = static_cast<int64_t>(chain.meta.config.maxValidationTimespanSeconds);
        if (longest > 0 && validFor > longest) {
          return pp::Error("The chain lets an operation wait at most " + std::to_string(longest / 3600) + " hours");
        }
        op::Common common;
        common.networkId = chain.networkId;
        common.genesisHash = chain.genesisHash;
        common.signers = signers.value();
        common.window = op::windowFrom(nowSeconds(), validFor);
        common.notes = notes;
        common.createdAt = nowSeconds();
        auto transfer = op::transfer(common, toId, amountValue, feeValue);
        if (!transfer) {
          return transfer.error();
        }
        auto least = link.minimumFee(transfer.value().record);
        if (least && feeValue < least.value()) {
          return pp::Error("The fee must be at least " + formatAmount(least.value()));
        }
        return transfer;
      },
      "transfer-" + std::to_string(amountValue) + "-to-" + std::to_string(toId) + ".json");
}

} // namespace apps::online
