#include "ui/VaultScreen.h"

#include "ui/UiApp.h"

#include <sodium.h>

#include <memory>

namespace apps::ui_host {

namespace {

void wipe(std::string &secret) {
  if (!secret.empty()) {
    sodium_memzero(secret.data(), secret.size());
    secret.clear();
  }
}

} // namespace

void VaultScreen::bind(ui::DataModelConstructor &model) {
  model.Bind("exists", &exists_);
  model.Bind("passphrase", &passphrase_);
  model.Bind("confirm", &confirm_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
  model.Bind("dir", &dir_);
  model.BindEventCallback("vault_unlock", &VaultScreen::unlock, this);
  model.BindEventCallback("vault_create", &VaultScreen::create, this);
}

void VaultScreen::willShow() {
  exists_ = session_.vault.exists();
  dir_ = session_.vault.dir();
  wipe(passphrase_);
  wipe(confirm_);
  error_.clear();
  notice_ = session_.lockedBecause;
  session_.lockedBecause.clear();
  busy_ = false;
  refresh();
}

void VaultScreen::finish(const std::string &error) {
  busy_ = false;
  wipe(passphrase_);
  wipe(confirm_);
  error_ = error;
  refresh();
  if (error.empty()) {
    session_.navigate(""); // the app's first screen
  }
}

void VaultScreen::unlock(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (busy_) {
    return;
  }
  if (passphrase_.empty()) {
    error_ = "Type your passphrase.";
    refresh();
    return;
  }
  busy_ = true;
  error_.clear();
  refresh();
  // Argon2id at sensitive limits takes seconds: off the UI thread.
  auto passphrase = std::make_shared<std::string>(passphrase_);
  auto result = std::make_shared<std::string>();
  app_->runInBackground(
      [this, passphrase, result] {
        if (auto unlocked = session_.vault.unlock(*passphrase); !unlocked) {
          *result = unlocked.error().message + ".";
        }
        wipe(*passphrase);
      },
      [this, result] { finish(*result); });
}

void VaultScreen::create(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (busy_) {
    return;
  }
  if (passphrase_.size() < Vault::kMinPassphraseLength) {
    error_ = "Choose a passphrase of at least " + std::to_string(Vault::kMinPassphraseLength) +
             " characters: a few words you will remember.";
    refresh();
    return;
  }
  if (passphrase_ != confirm_) {
    error_ = "The two passphrases differ.";
    refresh();
    return;
  }
  busy_ = true;
  error_.clear();
  refresh();
  auto passphrase = std::make_shared<std::string>(passphrase_);
  auto result = std::make_shared<std::string>();
  app_->runInBackground(
      [this, passphrase, result] {
        if (auto created = session_.vault.create(*passphrase); !created) {
          *result = created.error().message + ".";
        }
        wipe(*passphrase);
      },
      [this, result] { finish(*result); });
}

} // namespace apps::ui_host
