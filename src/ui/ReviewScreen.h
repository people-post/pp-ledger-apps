#pragma once
// An operation as its signers review it (DESIGN.md § The operation file): what it does — recomputed
// from its bytes —, its deadline, who signed, the creator's notes; signed with a vault key into a
// copy for the role app to merge.

#include "ui/Screen.h"
#include "ui/Session.h"

#include "core/Operations.h"

#include <optional>
#include <vector>

namespace admin::ui_host {

class ReviewScreen : public Screen {
public:
  explicit ReviewScreen(Session &session) : session_(session) {}
  void willShow() override;
  /** Open `path` for review (a file given on the command line). */
  void open(const std::string &path);

  struct Line {
    std::string label;
    std::string value;
  };
  struct Signer {
    std::string name;
    bool done{false};
  };
  struct SignKey {
    std::string id;
    std::string label;
  };

protected:
  std::string modelName() const override { return "review"; }
  std::string viewName() const override { return "review.rml"; }
  void bind(ui::DataModelConstructor &model) override;

private:
  void chooseFile(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void sign(ui::DataModelHandle, ui::Event &, const ui::VariantList &args);
  void close(ui::DataModelHandle, ui::Event &, const ui::VariantList &);
  void show();
  void say(const std::string &error, const std::string &notice = "");

  Session &session_;
  std::optional<Review> review_;
  std::string path_;

  bool loaded_{false};
  std::string file_;
  std::string title_;
  std::vector<Line> lines_;
  std::string deadline_;
  bool expired_{false};
  std::vector<Signer> signers_;
  std::string progress_;
  bool complete_{false};
  std::string notes_;
  std::vector<SignKey> signable_;
  std::string error_;
  std::string notice_;
};

} // namespace admin::ui_host
