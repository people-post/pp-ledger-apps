#include "ui/UiApp.h"

#include "ui/Screen.h"

#include <ui/core/Core.h>
#include <ui/dom/Context.h>
#include <ui/dom/Element.h>
#include <ui/dom/ElementDocument.h>

#include <SDL3/SDL.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

namespace admin::ui_host {

namespace {

/** What a file dialog's callback carries back to the UI thread. */
struct DialogRequest {
  UiApp *app;
  std::function<void(std::optional<std::string>)> done;
  std::vector<FileFilter> filters;
  std::vector<SDL_DialogFileFilter> sdlFilters;
};

void dialogDone(void *userdata, const char *const *files, int) {
  auto *request = static_cast<DialogRequest *>(userdata);
  std::optional<std::string> chosen;
  if (files && files[0]) {
    chosen = files[0];
  }
  request->app->post([request, chosen] {
    request->done(chosen);
    delete request;
  });
}

DialogRequest *newRequest(UiApp *app, const std::vector<FileFilter> &filters,
                          std::function<void(std::optional<std::string>)> done) {
  auto *request = new DialogRequest{app, std::move(done), filters, {}};
  for (const auto &filter : request->filters) {
    request->sdlFilters.push_back({filter.name.c_str(), filter.pattern.c_str()});
  }
  return request;
}

} // namespace

UiApp::UiApp(AppOptions options) : options_(std::move(options)) {}

UiApp::~UiApp() {
  if (context_) {
    ui::RemoveContext("main");
    context_ = nullptr;
  }
  if (engineStarted_) {
    ui::Shutdown();
  }
  host_.close();
}

AppOptions UiApp::parseArgs(int argc, char **argv, AppOptions defaults, std::vector<std::string> &args) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--screenshot" && i + 1 < argc) {
      defaults.screenshot = argv[++i];
    } else if (arg == "--frames" && i + 1 < argc) {
      defaults.screenshotFrames = std::atoi(argv[++i]);
#ifdef PP_ADMIN_DEV_HOOKS
    } else if (arg == "--script" && i + 1 < argc) {
      defaults.script = argv[++i];
#endif
    } else {
      args.push_back(arg);
    }
  }
  return defaults;
}

bool UiApp::start() {
  if (!host_.open(options_.title, options_.width, options_.height)) {
    return false;
  }
  ui::SetSystemInterface(host_.systemInterface());
  ui::SetRenderInterface(host_.renderInterface());
  if (!ui::Initialise()) {
    SDL_Log("Cannot start the UI engine");
    return false;
  }
  engineStarted_ = true;
  if (!ui::LoadFontFace(assetPath("fonts/LatoLatin-Regular.ttf"))) {
    SDL_Log("Cannot load the font from %s", assetPath("fonts").c_str());
    return false;
  }
  int width = 0;
  int height = 0;
  SDL_GetWindowSizeInPixels(host_.window(), &width, &height);
  context_ = ui::CreateContext("main", {width, height});
  if (!context_) {
    SDL_Log("Cannot create the UI context");
    return false;
  }
  context_->SetDensityIndependentPixelRatio(host_.densityRatio());
#ifdef PP_ADMIN_DEV_HOOKS
  if (!options_.script.empty()) {
    std::ifstream in(options_.script);
    for (std::string line; std::getline(in, line);) {
      if (!line.empty() && line.front() != '#') {
        script_.push_back(line);
      }
    }
  }
#endif
  return true;
}

std::string UiApp::assetPath(const std::string &relative) const {
  namespace fs = std::filesystem;
  if (const char *dir = std::getenv("PP_ADMIN_ASSETS"); dir && *dir) {
    return (fs::path(dir) / relative).string();
  }
  if (const char *base = SDL_GetBasePath()) {
#ifdef __APPLE__
    const fs::path bundled = fs::path(base) / "assets"; // <app>.app/Contents/Resources/
#else
    const fs::path bundled = fs::path(base) / ".." / "share" / "pp-ledger-admin" / "assets";
#endif
    std::error_code ec;
    if (fs::exists(bundled / relative, ec)) {
      return (bundled / relative).string();
    }
  }
  return (fs::path(PP_ADMIN_ASSETS_DIR) / relative).string();
}

ui::ElementDocument *UiApp::loadView(const std::string &view) {
  ui::ElementDocument *document = context_->LoadDocument(assetPath("views/" + view));
  if (!document) {
    SDL_Log("Cannot load the view %s", view.c_str());
  }
  return document;
}

void UiApp::show(Screen &screen) {
  if (shown_ == &screen) {
    screen.willShow();
    return;
  }
  if (shown_ && shown_->document()) {
    shown_->document()->Hide();
  }
  shown_ = &screen;
  screen.willShow();
  if (screen.document()) {
    screen.document()->Show();
  }
}

void UiApp::post(std::function<void()> fn) {
  {
    std::lock_guard lock(postedMutex_);
    posted_.push_back(std::move(fn));
  }
  SDL_Event wake{};
  wake.type = SDL_EVENT_USER;
  SDL_PushEvent(&wake);
}

void UiApp::drainPosted() {
  std::vector<std::function<void()>> pending;
  {
    std::lock_guard lock(postedMutex_);
    pending.swap(posted_);
  }
  for (auto &fn : pending) {
    fn();
  }
}

void UiApp::runInBackground(std::function<void()> work, std::function<void()> done) {
  ++backgroundJobs_;
  std::thread([this, work = std::move(work), done = std::move(done)]() mutable {
    work();
    post([this, done = std::move(done)] {
      --backgroundJobs_;
      done();
    });
  }).detach();
}

void UiApp::chooseFileToOpen(const std::vector<FileFilter> &filters,
                             std::function<void(std::optional<std::string>)> done) {
  if (!dialogAnswers_.empty()) { // a development script answers
    const std::string answer = dialogAnswers_.front();
    dialogAnswers_.erase(dialogAnswers_.begin());
    post([done = std::move(done), answer] { done(answer); });
    return;
  }
  auto *request = newRequest(this, filters, std::move(done));
  SDL_ShowOpenFileDialog(dialogDone, request, host_.window(), request->sdlFilters.data(),
                         static_cast<int>(request->sdlFilters.size()), nullptr, false);
}

void UiApp::chooseFileToSave(const std::vector<FileFilter> &filters, const std::string &suggested,
                             std::function<void(std::optional<std::string>)> done) {
  if (!dialogAnswers_.empty()) { // a development script answers
    const std::string answer = dialogAnswers_.front();
    dialogAnswers_.erase(dialogAnswers_.begin());
    post([done = std::move(done), answer] { done(answer); });
    return;
  }
  auto *request = newRequest(this, filters, std::move(done));
  SDL_ShowSaveFileDialog(dialogDone, request, host_.window(), request->sdlFilters.data(),
                         static_cast<int>(request->sdlFilters.size()),
                         suggested.empty() ? nullptr : suggested.c_str());
}

bool UiApp::stepScript() {
  if (scriptWait_ > 0) {
    --scriptWait_;
    return true;
  }
  while (scriptLine_ < script_.size()) {
    const std::string line = script_[scriptLine_];
    const auto space = line.find(' ');
    const std::string verb = line.substr(0, space);
    const std::string rest = space == std::string::npos ? "" : line.substr(space + 1);
    if (verb == "idle" && backgroundJobs_ > 0) {
      return true; // again next frame
    }
    ++scriptLine_;
    if (verb == "wait") {
      scriptWait_ = std::atoi(rest.c_str());
      return true;
    } else if (verb == "click") {
      ui::Element *element =
          shown_ && shown_->document() ? shown_->document()->GetElementById(rest.substr(rest.front() == '#')) : nullptr;
      if (!element) {
        SDL_Log("script: no element %s", rest.c_str());
        return false;
      }
      const ui::Vector2f at =
          element->GetAbsoluteOffset(ui::BoxArea::Border) + element->GetBox().GetSize(ui::BoxArea::Border) / 2.0f;
      context_->ProcessMouseMove(static_cast<int>(at.x), static_cast<int>(at.y), 0);
      context_->ProcessMouseButtonDown(0, 0);
      context_->ProcessMouseButtonUp(0, 0);
      scriptWait_ = 2;
      return true;
    } else if (verb == "type") {
      context_->ProcessTextInput(rest);
    } else if (verb == "answer") {
      dialogAnswers_.push_back(rest);
    } else if (verb == "shot") {
      context_->Update();
      host_.beginFrame();
      context_->Render();
      host_.endFrame();
      if (!host_.saveFrame(rest)) {
        SDL_Log("script: cannot save %s", rest.c_str());
      }
      host_.presentFrame();
    } else if (verb == "quit") {
      return false;
    }
  }
  return true;
}

int UiApp::run() {
  int frames = 0;
  while (running_) {
    if (!script_.empty() && !stepScript()) {
      break;
    }
    // Sleep until something happens, unless the UI animates, a screenshot is due or a script runs.
    const bool idle = options_.screenshot.empty() && script_.empty() && context_->GetNextUpdateDelay() > 0.1;
    if (!host_.processEvents(context_, idle)) {
      break;
    }
    drainPosted();
    context_->Update();
    host_.beginFrame();
    context_->Render();
    host_.endFrame();
    if (!options_.screenshot.empty() && ++frames >= options_.screenshotFrames && backgroundJobs_ == 0) {
      const bool saved = host_.saveFrame(options_.screenshot);
      host_.presentFrame();
      return saved ? 0 : 1;
    }
    host_.presentFrame();
  }
  // A worker may still post: let it finish before the app goes.
  while (backgroundJobs_ > 0) {
    SDL_Delay(10);
    drainPosted();
  }
  return 0;
}

} // namespace admin::ui_host
