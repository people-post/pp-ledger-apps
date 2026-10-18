#pragma once
// An app's frame: the window, the UI engine, assets, the main loop, work off the UI thread, native
// file dialogs. Screens (Screen.h) are shown in it, one at a time.

#include "ui/WindowHost.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ui {
class Context;
class ElementDocument;
} // namespace ui

namespace admin::ui_host {

class Screen;

struct AppOptions {
  std::string title;
  int width{1000};
  int height{720};
  /** Development: save the window as a PNG after `screenshotFrames` frames, then quit. */
  std::string screenshot;
  int screenshotFrames{30};
  /**
   * Development (PP_ADMIN_DEV_HOOKS): steps to drive the app, one per line — `click #id`,
   * `type <text>`, `wait <frames>`, `idle` (background work done), `answer <path>[|<path>…]` (the
   * next file dialog's choice), `sleep <ms>`, `expect #id` (fails the run unless the screen shown
   * has it), `shot <png>`, `quit`. A failed step ends the run with exit code 1.
   */
  std::string script;
};

/** A file type a dialog offers: "Operation files", "json". */
struct FileFilter {
  std::string name;
  std::string pattern;
};

class UiApp {
public:
  explicit UiApp(AppOptions options);
  ~UiApp();
  UiApp(const UiApp &) = delete;
  UiApp &operator=(const UiApp &) = delete;

  /** Take --screenshot <png> [--frames N] from the command line; the rest stay in `args`. */
  static AppOptions parseArgs(int argc, char **argv, AppOptions defaults, std::vector<std::string> &args);

  /** Open the window and start the UI engine; false (logged) when it cannot. */
  bool start();
  ui::Context *context() { return context_; }

  /**
   * A file under the app's assets: $PP_ADMIN_ASSETS, else the bundle's (packaged builds), else the
   * source tree's (development builds).
   */
  std::string assetPath(const std::string &relative) const;
  /** Load assets/views/`view` (hidden). */
  ui::ElementDocument *loadView(const std::string &view);

  /** Show `screen` (hiding the one shown). */
  void show(Screen &screen);
  Screen *shown() const { return shown_; }

  /** Run `fn` on the UI thread, at the next frame (from any thread). */
  void post(std::function<void()> fn);
  /** Run `work` on a worker thread, then `done` on the UI thread. */
  void runInBackground(std::function<void()> work, std::function<void()> done);

  /** Ask for a file to open; `done` gets it, or nothing when cancelled. */
  void chooseFileToOpen(const std::vector<FileFilter> &filters, std::function<void(std::optional<std::string>)> done);
  /** Ask for one or more files to open; `done` gets them (none when cancelled). */
  void chooseFilesToOpen(const std::vector<FileFilter> &filters, std::function<void(std::vector<std::string>)> done);
  /** Ask where to save; `suggested` is where the dialog starts. */
  void chooseFileToSave(const std::vector<FileFilter> &filters, const std::string &suggested,
                        std::function<void(std::optional<std::string>)> done);

  /** Call `onIdle` once nobody has touched the app for `seconds` (again after the next touch). */
  void setIdleTimeout(int seconds, std::function<void()> onIdle);

  /** The main loop, until the window closes or quit(); the exit code. */
  int run();
  void quit() { running_ = false; }
  /** False once the app is closing: long background work stops early. */
  bool running() const { return running_; }

private:
  void drainPosted();
  std::optional<std::vector<std::string>> takeAnswer();
  /** Advance the development script; false once it quits. */
  bool stepScript();

  AppOptions options_;
  WindowHost host_;
  ui::Context *context_{nullptr};
  bool engineStarted_{false};
  std::atomic<bool> running_{true};
  Screen *shown_{nullptr};
  std::mutex postedMutex_;
  std::vector<std::function<void()>> posted_;
  int backgroundJobs_{0};
  std::vector<std::string> script_;
  size_t scriptLine_{0};
  int scriptWait_{0};
  bool scriptFailed_{false};
  std::vector<std::string> dialogAnswers_;
  int idleSeconds_{0};
  std::function<void()> onIdle_;
  bool idleFired_{false};
};

} // namespace admin::ui_host
