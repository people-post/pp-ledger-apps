#include "online/GenesisScreens.h"

#include "net/AdminOperations.h"
#include "core/AddressBook.h"
#include "core/Amounts.h"
#include "core/Operations.h"
#include "ledger/chain/Genesis.h"

#include <cctype>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace apps::online {

namespace op = pp::operation;

namespace {

const std::vector<ui_host::FileFilter> kOperations = {{"Operation files", "json"}};

/** "2026-10-20 12:00" (local time) or "+N" hours from now; nullopt when neither. */
std::optional<int64_t> parseStart(const std::string &text, int64_t now) {
  if (!text.empty() && text.front() == '+') {
    auto hours = parseAmount(text.substr(1));
    return hours ? std::optional<int64_t>(now + static_cast<int64_t>(hours.value()) * 3600) : std::nullopt;
  }
  std::tm tm{};
  std::istringstream in(text);
  in >> std::get_time(&tm, "%Y-%m-%d %H:%M");
  if (in.fail()) {
    return std::nullopt;
  }
  tm.tm_isdst = -1;
  const std::time_t at = std::mktime(&tm);
  return at == -1 ? std::nullopt : std::optional<int64_t>(at);
}

std::string formatLocal(int64_t at) {
  const std::time_t t = static_cast<std::time_t>(at);
  std::tm tm{};
  localtime_r(&t, &tm);
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%d %H:%M");
  return out.str();
}

/** Keys of `role` people handed cards for, and the vault's own: (name, public key). */
std::vector<std::pair<std::string, std::string>> keysOfRole(ui_host::Session &session, Role role) {
  std::vector<std::pair<std::string, std::string>> out;
  auto seen = [&out](const std::string &key) {
    for (const auto &[name, k] : out) {
      if (k == key) {
        return true;
      }
    }
    return false;
  };
  if (auto book = AddressBook::load(session.vault)) {
    for (const auto &card : book.value().cards()) {
      if (card.role == role && !seen(card.publicKey)) {
        out.emplace_back(card.name, card.publicKey);
      }
    }
  }
  if (auto keys = session.vault.keys()) {
    for (const auto &key : keys.value()) {
      if (key.role == role && !seen(key.publicKey)) {
        out.emplace_back(key.label + ", your key", key.publicKey);
      }
    }
  }
  return out;
}

/** "Most of": the default how-many-sign of `n` keys. */
uint32_t majority(size_t n) { return n == 0 ? 0 : static_cast<uint32_t>(n / 2 + 1); }

} // namespace

// ---- The genesis plan ----

void GenesisPlanScreen::bind(ui::DataModelConstructor &model) {
  if (auto choice = model.RegisterStruct<KeyChoice>()) {
    choice.RegisterMember("name", &KeyChoice::name);
    choice.RegisterMember("fingerprint", &KeyChoice::fingerprint);
    choice.RegisterMember("chosen", &KeyChoice::chosen);
  }
  model.RegisterArray<std::vector<KeyChoice>>();
  if (auto section = model.RegisterStruct<Section>()) {
    section.RegisterMember("role", &Section::role);
    section.RegisterMember("title", &Section::title);
    section.RegisterMember("help", &Section::help);
    section.RegisterMember("min", &Section::min);
    section.RegisterMember("keys", &Section::keys);
  }
  model.RegisterArray<std::vector<Section>>();
  if (auto miner = model.RegisterStruct<Miner>()) {
    miner.RegisterMember("name", &Miner::name);
    miner.RegisterMember("fingerprint", &Miner::fingerprint);
    miner.RegisterMember("chosen", &Miner::chosen);
    miner.RegisterMember("stake", &Miner::stake);
  }
  model.RegisterArray<std::vector<Miner>>();
  if (auto setting = model.RegisterStruct<Setting>()) {
    setting.RegisterMember("name", &Setting::name);
    setting.RegisterMember("title", &Setting::title);
    setting.RegisterMember("value", &Setting::value);
  }
  model.RegisterArray<std::vector<Setting>>();
  model.Bind("network_id", &networkId_);
  model.Bind("start", &start_);
  model.Bind("sections", &sections_);
  model.Bind("beacons", &beacons_);
  model.Bind("miners", &miners_);
  model.Bind("settings", &settings_);
  model.Bind("names", &names_);
  model.Bind("recovery_delay", &recoveryDelay_);
  model.Bind("notes", &notes_);
  model.Bind("error", &error_);
  model.Bind("notice", &notice_);
  model.BindEventCallback("plan_make", &GenesisPlanScreen::makePlan, this);
}

void GenesisPlanScreen::say(const std::string &error, const std::string &notice) {
  error_ = error;
  notice_ = notice;
  refresh();
}

void GenesisPlanScreen::willShow() {
  // The key choices follow the address book (cards added since); what was typed stays.
  struct Place {
    Role role;
    const char *title;
    const char *help;
  };
  const Place places[] = {
      {Role::Admin, "Admin set", "Mints, changes the config, certifies beacon keys. At least 3 keys, 2 to sign."},
      {Role::Recovery, "Recovery set", "Replaces the admin set if its keys are lost or stolen. Not admin keys."},
      {Role::Reserve, "Reserve", "Holds the supply; funds the registrar and others."},
      {Role::Registrar, "Registrar", "Creates the accounts the chain issues."},
      {Role::Fee, "Fee account", "Collects the fees."},
      {Role::Recycle, "Recycle account", "Takes what ended accounts leave."},
  };
  std::vector<Section> sections;
  std::vector<std::vector<std::string>> sectionKeys;
  for (const auto &place : places) {
    Section section{roleName(place.role), place.title, place.help, "", {}};
    std::vector<std::string> keys;
    for (const auto &[name, key] : keysOfRole(session_, place.role)) {
      section.keys.push_back({name, op::fingerprint(key), true});
      keys.push_back(key);
    }
    // Most of the keys; the admin set at least 2 (pp-ledger's shape), never more than there are.
    const uint32_t most = majority(keys.size());
    section.min = std::to_string(place.role == Role::Admin
                                     ? std::min<uint32_t>(std::max<uint32_t>(2, most), static_cast<uint32_t>(keys.size()))
                                     : most);
    // Keep what was chosen and typed before, for the keys still offered.
    for (size_t s = 0; s < sections_.size(); ++s) {
      if (sections_[s].role != section.role) {
        continue;
      }
      section.min = sections_[s].min;
      for (size_t i = 0; i < section.keys.size(); ++i) {
        for (size_t j = 0; j < sectionKeys_[s].size(); ++j) {
          if (sectionKeys_[s][j] == keys[i]) {
            section.keys[i].chosen = sections_[s].keys[j].chosen;
          }
        }
      }
    }
    sections.push_back(std::move(section));
    sectionKeys.push_back(std::move(keys));
  }
  sections_ = std::move(sections);
  sectionKeys_ = std::move(sectionKeys);

  beacons_.clear();
  beaconKeys_.clear();
  for (const auto &[name, key] : keysOfRole(session_, Role::Beacon)) {
    beacons_.push_back({name, op::fingerprint(key), beaconKeys_.empty()});
    beaconKeys_.push_back(key);
  }
  const auto miners = keysOfRole(session_, Role::Miner);
  if (minerKeys_.size() != miners.size()) {
    miners_.clear();
    minerKeys_.clear();
    const std::string stake = formatAmount(pp::chain_genesis::defaultMinerStake(miners.size()));
    for (const auto &[name, key] : miners) {
      miners_.push_back({name, op::fingerprint(key), true, stake});
      minerKeys_.push_back(key);
    }
  }
  if (!filled_) {
    filled_ = true;
    const auto config = pp::chain_genesis::defaultConfig("");
    for (const auto &field : net::genesisFields()) {
      settings_.push_back({field.name, field.title, formatAmount(field.get(config))});
    }
    recoveryDelay_ = formatAmount(pp::chain_genesis::defaultRecoveryDelayEpochs(config));
    const auto names = pp::chain_genesis::defaultSystemNames();
    names_ = {{"genesis", "Genesis — the native token's symbol", names.genesis},
              {"reserve", "Reserve", names.reserve},
              {"registrar", "Registrar", names.registrar},
              {"fee", "Fee account", names.fee},
              {"recycle", "Recycle account", names.recycle}};
    const int64_t tomorrow = (nowSeconds() / 3600 + 24) * 3600;
    start_ = formatLocal(tomorrow);
  }
  say("");
}

std::vector<std::string> GenesisPlanScreen::chosenKeys(size_t section) const {
  std::vector<std::string> keys;
  for (size_t i = 0; i < sections_[section].keys.size(); ++i) {
    if (sections_[section].keys[i].chosen) {
      keys.push_back(sectionKeys_[section][i]);
    }
  }
  return keys;
}

void GenesisPlanScreen::makePlan(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  net::GenesisForm form;
  form.config = pp::chain_genesis::defaultConfig(networkId_);
  for (const auto &setting : settings_) {
    auto value = parseAmount(setting.value);
    if (!value) {
      say(setting.title + ": " + value.error().message + ".");
      return;
    }
    for (const auto &field : net::genesisFields()) {
      if (field.name == setting.name) {
        field.set(form.config, value.value());
      }
    }
  }
  const auto start = parseStart(start_, nowSeconds());
  if (!start) {
    say("The start: a date and time like 2026-10-20 12:00, or +24 for a day from now.");
    return;
  }
  form.config.genesisTime = *start;
  for (size_t s = 0; s < sections_.size(); ++s) {
    auto min = parseAmount(sections_[s].min);
    net::PlanKeys keys{chosenKeys(s), min ? static_cast<uint32_t>(min.value()) : 0};
    if (keys.publicKeys.empty() || keys.minSignatures == 0 || keys.minSignatures > keys.publicKeys.size()) {
      say(sections_[s].title + ": choose its keys, and between 1 and that many to sign.");
      return;
    }
    const std::string role = sections_[s].role;
    if (role == "admin") {
      form.admin = keys;
    } else if (role == "recovery") {
      form.recovery = keys;
    } else if (role == "reserve") {
      form.reserve = keys;
    } else if (role == "registrar") {
      form.registrar = keys;
    } else if (role == "fee") {
      form.fee = keys;
    } else if (role == "recycle") {
      form.recycle = keys;
    }
  }
  for (size_t i = 0; i < beacons_.size(); ++i) {
    if (beacons_[i].chosen) {
      form.beaconKey = beaconKeys_[i];
      break;
    }
  }
  for (size_t i = 0; i < miners_.size(); ++i) {
    if (!miners_[i].chosen) {
      continue;
    }
    auto stake = parseAmount(miners_[i].stake);
    if (!stake || stake.value() == 0) {
      say("A genesis miner's stake: " + miners_[i].name + ".");
      return;
    }
    form.miners.emplace_back(minerKeys_[i], stake.value());
  }
  auto delay = parseAmount(recoveryDelay_);
  form.recoveryDelayEpochs = delay ? delay.value() : 0;
  for (const auto &named : names_) {
    std::string domain;
    for (char c : named.value) {
      if (c != ' ') {
        domain.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
      }
    }
    std::string *slot = named.name == "genesis"     ? &form.names.genesis
                        : named.name == "reserve"   ? &form.names.reserve
                        : named.name == "registrar" ? &form.names.registrar
                        : named.name == "fee"       ? &form.names.fee
                                                    : &form.names.recycle;
    *slot = domain;
  }
  auto plan = net::makeGenesisPlan(form, notes_, nowSeconds());
  if (!plan) {
    say(plan.error().message + ".");
    return;
  }
  auto made = std::make_shared<op::Operation>(std::move(plan.value()));
  app_->chooseFileToSave(kOperations, "genesis-" + networkId_ + ".json", [this, made](std::optional<std::string> path) {
    if (!path) {
      say("", "Not saved.");
      return;
    }
    if (auto saved = op::save(*made, *path); !saved) {
      say(saved.error().message);
      return;
    }
    say("", "Saved " + std::filesystem::path(*path).filename().string() +
                ": hand a copy to each admin; the beacon's operator starts the chain from the signed one "
                "(pp-beacon --init --genesis-op).");
    made_(*path);
  });
}

// ---- Recovery ----

void RecoveryScreen::bind(ui::DataModelConstructor &model) {
  bindForm(model);
  if (auto choice = model.RegisterStruct<KeyChoice>()) {
    choice.RegisterMember("name", &KeyChoice::name);
    choice.RegisterMember("fingerprint", &KeyChoice::fingerprint);
    choice.RegisterMember("chosen", &KeyChoice::chosen);
  }
  model.RegisterArray<std::vector<KeyChoice>>();
  model.Bind("recovery", &recovery_);
  model.Bind("admins", &admins_);
  model.Bind("candidates", &candidates_);
  model.Bind("threshold", &threshold_);
  model.BindEventCallback("recovery_make", &RecoveryScreen::makeRecovery, this);
}

void RecoveryScreen::willShow() {
  say("", "Reading the chain's authority…");
  auto meta = std::make_shared<pp::GenesisAccountMeta>();
  online_.withChain(
      [meta](net::ChainLink &link) -> std::string {
        auto chain = link.info();
        if (!chain) {
          return chain.error().message;
        }
        *meta = chain.value().meta;
        return "";
      },
      [this, meta](const std::string &error) {
        candidates_.clear();
        candidateKeys_.clear();
        for (const auto &[name, key] : keysOfRole(session_, Role::Admin)) {
          const auto &inForce = meta->genesis.wallet.publicKeys;
          if (std::find(inForce.begin(), inForce.end(), key) != inForce.end()) {
            continue; // the lost set is not offered back
          }
          candidates_.push_back({name, op::fingerprint(key), false});
          candidateKeys_.push_back(key);
        }
        if (!error.empty()) {
          recovery_ = admins_ = "unknown";
          say(error + ".");
          return;
        }
        const auto &recovery = meta->authority.recovery;
        recovery_ = std::to_string(recovery.minSignatures) + " of " + std::to_string(recovery.publicKeys.size()) +
                    " sign; the new set applies " + std::to_string(recovery.delayEpochs) + " epoch(s) after";
        admins_ = std::to_string(meta->genesis.wallet.minSignatures) + " of " +
                  std::to_string(meta->genesis.wallet.publicKeys.size());
        if (meta->authority.hasPendingRecovery) {
          say("A recovery is pending already: it applies from epoch " +
              formatAmount(meta->authority.pendingRecovery.applyEpoch) + ".");
          return;
        }
        say("");
      });
}

void RecoveryScreen::makeRecovery(ui::DataModelHandle, ui::Event &, const ui::VariantList &) {
  std::vector<std::string> keys;
  for (size_t i = 0; i < candidates_.size(); ++i) {
    if (candidates_[i].chosen) {
      keys.push_back(candidateKeys_[i]);
    }
  }
  auto threshold = parseAmount(threshold_);
  if (keys.size() < 3 || !threshold || threshold.value() < 2 || threshold.value() > keys.size()) {
    say("Choose at least 3 new admin keys, and between 2 and that many to sign.");
    return;
  }
  const auto min = static_cast<uint32_t>(threshold.value());
  make(
      [keys, min](net::ChainLink &, const net::ChainInfo &chain, int64_t validFor, const std::string &notes) {
        return net::makeRecovery(chain, keys, min, {nowSeconds(), validFor, notes});
      },
      "recovery.json");
}

} // namespace apps::online
