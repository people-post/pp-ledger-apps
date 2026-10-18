#include "ui/ReviewScreen.h"

#include "ui/UiApp.h"

#include "lib/common/Utilities.h"

#include <chrono>
#include <filesystem>

namespace admin::ui_host {

namespace op = pp::operation;

namespace {

const std::vector<FileFilter> kOperations = {{"Operation files", "json"}};

int64_t now() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

int indexArg(const ui::VariantList &args) { return args.empty() ? -1 : args[0].Get<int>(-1); }

} // namespace

void ReviewScreen::bind(ui::DataModelConstructor &model) {
  if (auto line = model.RegisterStruct<Line>()) {
    line.RegisterMember("label", &Line::label);
    line.RegisterMember("value", &Line::value);
  }
  if (auto signer = model.RegisterStruct<Signer>()) {
    signer.RegisterMember("name", &Signer::name);
    signer.RegisterMember("done", &Signer::done);
  }
  if (auto key = model.RegisterStruct<SignKey>()) {
    key.RegisterMember("id", &SignKey::id);
    key.RegisterMember("label", &SignKey::label);
  }
  model.RegisterArray<std::vector<Line>>();
  model.RegisterArray<std::vector<Signer>>();
  model.RegisterArray<std::vector<SignKey>>();
  model.Bind("loaded", &loaded_);
  model.Bind("file", &file_);
  model.Bind("title", &title_);
  model.Bind("lines", &lines_);
  model.Bind("deadline", &deadline_);
  model.Bind("expired", &expired_);
  model.Bind("signers", &signers_);
  model.Bind("progress", &progress_);
  model.Bind("complete", &complete_);
  model.Bind("notes", &notes_);
  model.Bind("signable", &signable_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.BindEventCallback("review_open", &ReviewScreen::chooseFile, this);
  model.BindEventCallback("review_sign", &ReviewScreen::sign, this);
  model.BindEventCallback("review_close", &ReviewScreen::close, this);
}

void ReviewScreen::willShow() { show(); }

void ReviewScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void ReviewScreen::open(const std::string &path) {
  auto review = openOperation(path);
  if (!review) {
    review_.reset();
    show();
    say(review.error().message);
    return;
  }
  review_ = std::move(review.value());
  path_ = path;
  error_.clear();
  notice_.clear();
  show();
}

void ReviewScreen::show() {
  loaded_ = review_.has_value();
  lines_.clear();
  signers_.clear();
  signable_.clear();
  if (!review_) {
    refresh();
    return;
  }
  const auto &summary = review_->summary;
  const auto &operation = review_->operation;
  file_ = std::filesystem::path(path_).filename().string();
  title_ = summary.title;
  for (const auto &[label, value] : summary.lines) {
    lines_.push_back({label, value});
  }
  expired_ = expired(summary, now());
  deadline_ = summary.validUntil == 0 ? "No deadline"
                                      : "Sign by " + pp::utl::formatTimestampLocal(summary.validUntil);
  const auto vaultKeys = session_.vault.keys();
  const std::vector<KeyInfo> keys = vaultKeys ? vaultKeys.value() : std::vector<KeyInfo>{};
  const auto book = AddressBook::load(session_.vault);
  const AddressBook names = book ? book.value() : AddressBook{};
  const auto check = op::check(operation);
  for (const auto &key : operation.signers.publicKeys) {
    const bool done = std::find(check.signedBy.begin(), check.signedBy.end(), key) != check.signedBy.end();
    signers_.push_back({signerName(key, names, keys), done});
  }
  progress_ = std::to_string(check.signedBy.size()) + " of " + std::to_string(check.needed);
  complete_ = check.complete();
  notes_ = operation.notes;
  for (const auto &key : keysToSign(operation, keys)) {
    signable_.push_back({key.id, key.label});
  }
  refresh();
}

void ReviewScreen::chooseFile(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  app_->chooseFileToOpen(kOperations, [this](std::optional<std::string> path) {
    if (path) {
      open(*path);
    }
  });
}

void ReviewScreen::close(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  review_.reset();
  path_.clear();
  error_.clear();
  notice_.clear();
  show();
}

void ReviewScreen::sign(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
  const int index = indexArg(args);
  if (!review_ || index < 0 || index >= static_cast<int>(signable_.size())) {
    return;
  }
  if (expired_) {
    say("This operation has expired: ask for a new one.");
    return;
  }
  op::Operation signedCopy = review_->operation;
  const std::string keyId = signable_[static_cast<size_t>(index)].id;
  if (auto signedIt = signWithVault(session_.vault, keyId, signedCopy); !signedIt) {
    say(signedIt.error().message);
    return;
  }
  auto key = session_.vault.key(keyId);
  const std::string suggested = signedCopyPath(path_, key ? key.value().publicKey : std::string());
  app_->chooseFileToSave(kOperations, suggested, [this, signedCopy](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    if (auto saved = op::save(signedCopy, *path); !saved) {
      say(saved.error().message);
      return;
    }
    review_->operation = signedCopy;
    review_->signatures = op::check(signedCopy);
    path_ = *path;
    show();
    say("", "Signed. Send " + std::filesystem::path(*path).filename().string() +
                " back to whoever collects the signatures.");
  });
}

} // namespace admin::ui_host
