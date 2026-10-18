#include "ui/ReviewScreen.h"

#include "ui/UiApp.h"

#include "lib/common/Utilities.h"

#include <algorithm>
#include <chrono>
#include <regex>
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
  model.Bind("unsaved", &unsaved_);
  model.Bind("can_submit", &canSubmit_);
  model.Bind("submitting", &submitting_);
  model.Bind("submit_status", &submitStatus_);
  model.Bind("submitted", &submitted_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.BindEventCallback("review_open", &ReviewScreen::chooseFile, this);
  model.BindEventCallback("review_sign", &ReviewScreen::sign, this);
  model.BindEventCallback("review_close", &ReviewScreen::close, this);
  model.BindEventCallback("review_add_copies", &ReviewScreen::addCopies, this);
  model.BindEventCallback("review_save", &ReviewScreen::saveMerged, this);
  model.BindEventCallback("review_submit", &ReviewScreen::submit, this);
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
  unsaved_ = false;
  submitStatus_.clear();
  submitted_ = false;
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
  const auto vaultKeys = session_.vault.keys();
  const std::vector<KeyInfo> keys = vaultKeys ? vaultKeys.value() : std::vector<KeyInfo>{};
  const auto book = AddressBook::load(session_.vault);
  const AddressBook names = book ? book.value() : AddressBook{};
  // Keys people know by name: "a1b2-…" becomes "Alice Chen (a1b2-…)".
  std::vector<std::pair<std::string, std::string>> known;
  for (const auto &card : names.cards()) {
    known.emplace_back(op::fingerprint(card.publicKey), card.name);
  }
  for (const auto &key : keys) {
    known.emplace_back(op::fingerprint(key.publicKey), key.label + ", your key");
  }
  for (const auto &[label, value] : summary.lines) {
    std::string named = value;
    for (const auto &[fingerprint, name] : known) {
      if (const auto at = named.find(fingerprint); at != std::string::npos) {
        named.replace(at, fingerprint.size(), name + " (" + fingerprint + ")");
      }
    }
    lines_.push_back({label, named});
  }
  std::string starts;
  if (operation.kind == op::Kind::Genesis) {
    // Who holds the chain first (keys, accounts: "2 of 3: …"), then its settings.
    static const std::regex keys(R"(\d+ of \d+:)");
    std::stable_partition(lines_.begin(), lines_.end(), [](const Line &line) {
      return line.label == "Network" || line.label == "Starts" || std::regex_search(line.value, keys);
    });
    for (const auto &line : lines_) {
      if (line.label == "Starts") {
        starts = "Starts " + line.value;
      }
    }
  }
  expired_ = expired(summary, now());
  deadline_ = summary.validUntil != 0 ? "Sign by " + pp::utl::formatTimestampLocal(summary.validUntil)
              : !starts.empty()       ? starts
                                      : "No deadline";
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
  canSubmit_ = static_cast<bool>(session_.submit) && complete_ && !expired_ && operation.kind != op::Kind::Genesis;
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
  unsaved_ = false;
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
    unsaved_ = false;
    show();
    say("", "Signed. Send " + std::filesystem::path(*path).filename().string() +
                " back to whoever collects the signatures.");
  });
}

void ReviewScreen::addCopies(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (!review_) {
    return;
  }
  app_->chooseFilesToOpen(kOperations, [this](std::vector<std::string> paths) {
    if (paths.empty() || !review_) {
      return;
    }
    auto added = admin::addCopies(*review_, paths);
    if (!added) {
      say(added.error().message + ".");
      return;
    }
    const size_t gained = added.value();
    unsaved_ = gained > 0;
    show();
    say("", gained == 0 ? "No new signatures in those copies."
                        : "Added " + std::to_string(gained) + " signature(s). Save the merged operation.");
  });
}

void ReviewScreen::saveMerged(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (!review_) {
    return;
  }
  std::filesystem::path suggested(path_);
  suggested.replace_filename(suggested.stem().string() + ".merged" + suggested.extension().string());
  app_->chooseFileToSave(kOperations, suggested.string(), [this](std::optional<std::string> path) {
    if (!path || !review_) {
      return;
    }
    if (auto saved = op::save(review_->operation, *path); !saved) {
      say(saved.error().message);
      return;
    }
    path_ = *path;
    unsaved_ = false;
    show();
    say("", review_->signatures.complete()
                ? "Saved " + std::filesystem::path(*path).filename().string() + ": complete, ready to submit."
                : "Saved " + std::filesystem::path(*path).filename().string() + ".");
  });
}

void ReviewScreen::submit(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (!review_ || !canSubmit_ || submitting_) {
    return;
  }
  submitting_ = true;
  submitted_ = false;
  submitStatus_ = "Sending…";
  say("", "");
  session_.submit(review_->operation, [this](const Session::SubmitProgress &progress) {
    submitStatus_ = progress.status;
    if (progress.done || progress.failed) {
      submitting_ = false;
      submitted_ = progress.done;
    }
    refresh();
  });
}

} // namespace admin::ui_host
