#include "ui/KeysScreen.h"

#include "ui/UiApp.h"

#include "core/AddressBook.h"
#include "core/AtomicFile.h"
#include "core/KeyCard.h"
#include "lib/common/Utilities.h"
#include "operation/Operation.h"

#include <sodium.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>

namespace apps::ui_host {

namespace {

const std::vector<FileFilter> kKeyCards = {{"Key cards", "json"}};
const std::vector<FileFilter> kKeyFiles = {{"Private keys (pp-client keygen)", "key"}};
const std::vector<FileFilter> kBackups = {{"Vault backups", "ppbackup"}};

std::string trimmed(std::string text) {
  text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); }), text.end());
  return text;
}

int indexArg(const ui::VariantList &args) { return args.empty() ? -1 : args[0].Get<int>(-1); }

} // namespace

void KeysScreen::bind(ui::DataModelConstructor &model) {
  if (auto row = model.RegisterStruct<KeyRow>()) {
    row.RegisterMember("id", &KeyRow::id);
    row.RegisterMember("label", &KeyRow::label);
    row.RegisterMember("role", &KeyRow::role);
    row.RegisterMember("fingerprint", &KeyRow::fingerprint);
  }
  if (auto row = model.RegisterStruct<CardRow>()) {
    row.RegisterMember("name", &CardRow::name);
    row.RegisterMember("role", &CardRow::role);
    row.RegisterMember("fingerprint", &CardRow::fingerprint);
  }
  model.RegisterArray<std::vector<KeyRow>>();
  model.RegisterArray<std::vector<CardRow>>();
  model.Bind("keys", &keys_);
  model.Bind("cards", &cards_);
  model.Bind("new_label", &newLabel_);
  model.Bind("new_role", &newRole_);
  model.Bind("backup_passphrase", &backupPassphrase_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.Bind("busy", &busy_);
  model.BindEventCallback("keys_generate", &KeysScreen::generate, this);
  model.BindEventCallback("keys_import", &KeysScreen::importKey, this);
  model.BindEventCallback("keys_export_card", &KeysScreen::exportCard, this);
  model.BindEventCallback("keys_add_card", &KeysScreen::addCard, this);
  model.BindEventCallback("keys_remove_card", &KeysScreen::removeCard, this);
  model.BindEventCallback("keys_backup", &KeysScreen::backup, this);
  model.BindEventCallback("keys_restore", &KeysScreen::restore, this);
}

void KeysScreen::willShow() {
  error_.clear();
  notice_.clear();
  reload();
}

void KeysScreen::reload() {
  keys_.clear();
  cards_.clear();
  cardKeys_.clear();
  if (auto keys = session_.vault.keys()) {
    for (const auto &key : keys.value()) {
      keys_.push_back({key.id, key.label, roleTitle(key.role), pp::operation::fingerprint(key.publicKey)});
    }
  } else {
    error_ = keys.error().message;
  }
  if (auto book = AddressBook::load(session_.vault)) {
    for (const auto &card : book.value().cards()) {
      cards_.push_back({card.name, roleTitle(card.role), pp::operation::fingerprint(card.publicKey)});
      cardKeys_.push_back(card.publicKey);
    }
  } else {
    error_ = book.error().message;
  }
  refresh();
}

void KeysScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void KeysScreen::generate(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto role = roleFromName(newRole_);
  if (newLabel_.empty() || !role) {
    say("Name the key (whose it is, what for) and choose its role.");
    return;
  }
  auto key = session_.vault.generateKey(newLabel_, *role);
  if (!key) {
    say(key.error().message);
    return;
  }
  newLabel_.clear();
  reload();
  say("", "Made key " + pp::operation::fingerprint(key.value().publicKey) +
              ". Back up the vault now: a lost key cannot be made again.");
}

void KeysScreen::importKey(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  const auto role = roleFromName(newRole_);
  if (!role) {
    say("Choose the key's role first.");
    return;
  }
  app_->chooseFileToOpen(kKeyFiles, [this, role = *role](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    // pp-client keygen -o <name> writes <name>.key and <name>.pub side by side.
    const std::filesystem::path keyPath(*path);
    const std::filesystem::path pubPath = std::filesystem::path(keyPath).replace_extension(".pub");
    auto privateKey = pp::utl::readPrivateKey(keyPath.string(), "");
    auto pubText = readFile(pubPath.string());
    if (!privateKey || !pubText) {
      say("Cannot read " + keyPath.filename().string() + " and " + pubPath.filename().string() +
          " (pp-client keygen writes both).");
      return;
    }
    std::string secret = std::move(privateKey.value());
    const std::string label = newLabel_.empty() ? keyPath.stem().string() : newLabel_;
    auto key = session_.vault.importKey(label, role, secret, pp::utl::hexDecode(trimmed(pubText.value())));
    sodium_memzero(secret.data(), secret.size());
    if (!key) {
      say(key.error().message);
      return;
    }
    newLabel_.clear();
    reload();
    say("", "Imported " + label + ". Delete " + keyPath.filename().string() +
                " from where it was: the vault keeps it encrypted.");
  });
}

void KeysScreen::exportCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
  const int index = indexArg(args);
  if (index < 0 || index >= static_cast<int>(keys_.size())) {
    return;
  }
  auto key = session_.vault.key(keys_[static_cast<size_t>(index)].id);
  if (!key) {
    say(key.error().message);
    return;
  }
  KeyCard card{key.value().label, key.value().role, key.value().publicKey};
  std::string suggested = key.value().label;
  std::replace_if(suggested.begin(), suggested.end(), [](unsigned char c) { return !std::isalnum(c); }, '-');
  app_->chooseFileToSave(kKeyCards, suggested + ".card.json", [this, card](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    if (auto saved = saveKeyCard(card, *path); !saved) {
      say(saved.error().message);
      return;
    }
    say("", "Key card saved: hand it to whoever builds the operations you sign.");
  });
}

void KeysScreen::addCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  app_->chooseFileToOpen(kKeyCards, [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    auto card = loadKeyCard(*path);
    if (!card) {
      say(card.error().message);
      return;
    }
    auto book = AddressBook::load(session_.vault);
    if (!book) {
      say(book.error().message);
      return;
    }
    book.value().put(card.value());
    if (auto saved = book.value().save(session_.vault); !saved) {
      say(saved.error().message);
      return;
    }
    reload();
    say("", "Added " + card.value().name + " (" + pp::operation::fingerprint(card.value().publicKey) +
                "). Check the fingerprint with them.");
  });
}

void KeysScreen::removeCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &args) {
  const int index = indexArg(args);
  if (index < 0 || index >= static_cast<int>(cardKeys_.size())) {
    return;
  }
  auto book = AddressBook::load(session_.vault);
  if (!book) {
    say(book.error().message);
    return;
  }
  book.value().remove(cardKeys_[static_cast<size_t>(index)]);
  if (auto saved = book.value().save(session_.vault); !saved) {
    say(saved.error().message);
    return;
  }
  reload();
}

void KeysScreen::backup(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (backupPassphrase_.size() < Vault::kMinPassphraseLength) {
    say("Choose a backup passphrase of at least " + std::to_string(Vault::kMinPassphraseLength) +
        " characters, and keep it apart from the backup.");
    return;
  }
  app_->chooseFileToSave(kBackups, "pp-ledger-apps.ppbackup", [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    busy_ = true;
    say("", "Writing the backup…");
    auto passphrase = std::make_shared<std::string>(backupPassphrase_);
    auto result = std::make_shared<std::string>();
    app_->runInBackground(
        [this, passphrase, result, path = *path] {
          if (auto written = session_.vault.exportBackup(path, *passphrase); !written) {
            *result = written.error().message;
          }
          sodium_memzero(passphrase->data(), passphrase->size());
        },
        [this, result] {
          busy_ = false;
          sodium_memzero(backupPassphrase_.data(), backupPassphrase_.size());
          backupPassphrase_.clear();
          say(*result, result->empty() ? "Backup written. Keep it on a stick, away from this machine." : "");
        });
  });
}

void KeysScreen::restore(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  if (backupPassphrase_.empty()) {
    say("Type the backup's passphrase first.");
    return;
  }
  app_->chooseFileToOpen(kBackups, [this](std::optional<std::string> path) {
    if (!path) {
      return;
    }
    busy_ = true;
    say("", "Reading the backup…");
    auto passphrase = std::make_shared<std::string>(backupPassphrase_);
    auto result = std::make_shared<std::string>();
    auto added = std::make_shared<size_t>(0);
    app_->runInBackground(
        [this, passphrase, result, added, path = *path] {
          if (auto restored = session_.vault.importBackup(path, *passphrase)) {
            *added = restored.value().size();
          } else {
            *result = restored.error().message;
          }
          sodium_memzero(passphrase->data(), passphrase->size());
        },
        [this, result, added] {
          busy_ = false;
          sodium_memzero(backupPassphrase_.data(), backupPassphrase_.size());
          backupPassphrase_.clear();
          reload();
          say(*result, result->empty() ? "Restored " + std::to_string(*added) + " key(s)." : "");
        });
  });
}

} // namespace apps::ui_host
