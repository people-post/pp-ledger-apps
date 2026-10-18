#pragma once
// The Token app's screens (projects/token-app/DESIGN.md § What the app does): the issuer's overview,
// issuing its token, sending from it, replacing its keys. The issuer is the account the operators run
// (its number kept in the vault, like the node's address); each form builds against the chain
// (net/TokenOperations).

#include "online/OperationForm.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace admin::online {

/** The issuer account the app operates: its number, kept in the vault. */
class Issuer {
public:
  explicit Issuer(ui_host::Session &session) : session_(session) {}

  /** The vault file its number is kept in. */
  static constexpr const char *kSettingsFile = "token.json";

  /** Read it from the vault (after it unlocks). */
  void load();
  /** None until the operators set it (Overview). */
  std::optional<uint64_t> account() const { return account_; }
  pp::Roe<void> save(uint64_t account);

private:
  ui_host::Session &session_;
  std::optional<uint64_t> account_;
};

class TokenOverviewScreen : public ui_host::Screen {
public:
  TokenOverviewScreen(Online &online, ui_host::Session &session, Issuer &issuer)
      : online_(online), session_(session), issuer_(issuer) {}
  void willShow() override;

  struct Fact {
    std::string label;
    std::string value;
  };
  struct Activity {
    std::string what;
    std::string fee;
  };

protected:
  std::string modelName() const override { return "token"; }
  std::string viewName() const override { return "token.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void saveAccount(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void say(const std::string &error, const std::string &notice = "");

  Online &online_;
  ui_host::Session &session_;
  Issuer &issuer_;
  std::string account_;
  bool known_{false};
  std::vector<Fact> facts_;
  std::vector<std::string> signers_;
  std::vector<Activity> recent_;
  std::string warning_;
  std::string error_;
  std::string notice_;
  bool busy_{false};
};

/** New tokens to a holder: a transfer of the issuer's own token. */
class IssueScreen : public OperationForm {
public:
  IssueScreen(Online &online, Issuer &issuer, std::function<void(const std::string &)> made)
      : OperationForm(online, std::move(made)), issuer_(issuer) {}
  void willShow() override;

protected:
  std::string modelName() const override { return "issue"; }
  std::string viewName() const override { return "issue.rml"; }
  void bind(ui::DataModelConstructor &model) override;
  void clearForm() override;

private:
  void makeIssue(ui::DataModelHandle, ui::Event &, const ui::VariantList &);

  Issuer &issuer_;
  std::string issuerName_;
  std::string supply_;
  std::string to_;
  std::string amount_;
  std::string fee_;
};

/** The native token, or a token the issuer holds, sent to another account. */
class SendScreen : public OperationForm {
public:
  SendScreen(Online &online, Issuer &issuer, std::function<void(const std::string &)> made)
      : OperationForm(online, std::move(made)), issuer_(issuer) {}
  void willShow() override;

protected:
  std::string modelName() const override { return "send"; }
  std::string viewName() const override { return "send.rml"; }
  void bind(ui::DataModelConstructor &model) override;
  void clearForm() override;

private:
  void makeSend(ui::DataModelHandle, ui::Event &, const ui::VariantList &);

  Issuer &issuer_;
  std::string holdings_;
  std::string token_;
  std::string to_;
  std::string amount_;
  std::string fee_;
};

/** The issuer's keys and threshold replaced, signed by the keys in force. */
class SignersScreen : public OperationForm {
public:
  SignersScreen(Online &online, ui_host::Session &session, Issuer &issuer,
                std::function<void(const std::string &)> made)
      : OperationForm(online, std::move(made)), session_(session), issuer_(issuer) {}
  void willShow() override;

  struct Candidate {
    std::string name;
    std::string fingerprint;
    bool chosen{false};
  };

protected:
  std::string modelName() const override { return "signers"; }
  std::string viewName() const override { return "signers.rml"; }
  void bind(ui::DataModelConstructor &model) override;
  void clearForm() override;

private:
  void addCard(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void updateFee(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void makeReplacement(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void addCandidate(const std::string &publicKey, bool chosen);
  std::vector<std::string> chosenKeys() const;

  ui_host::Session &session_;
  Issuer &issuer_;
  std::string current_;
  std::vector<Candidate> candidates_;
  std::vector<std::string> candidateKeys_;
  std::string threshold_;
  std::string fee_;
};

} // namespace admin::online
