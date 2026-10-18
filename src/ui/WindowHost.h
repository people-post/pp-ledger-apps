#pragma once
// The window an app runs in: SDL3 window, OpenGL 3.3 context, events into the UI context, frames.
// pp-cpp-ui leaves the host to the app; this follows its reference SDL/GL3 backend.

#include <ui/paint/RenderInterface.h>
#include <ui/base/SystemInterface.h>

#include <memory>
#include <string>

struct SDL_Window;
union SDL_Event;

namespace ui {
class Context;
}

namespace admin::ui_host {

class WindowHost {
public:
  WindowHost();
  ~WindowHost();
  WindowHost(const WindowHost &) = delete;
  WindowHost &operator=(const WindowHost &) = delete;

  /** Open the window (`width` x `height` in display-independent pixels). */
  bool open(const std::string &title, int width, int height);
  void close();

  ui::SystemInterface *systemInterface();
  ui::RenderInterface *renderInterface();
  SDL_Window *window() const;
  /** Pixels per dp: the context's density ratio. */
  float densityRatio() const;

  /** Feed pending events to `context`; false once the window is closed. */
  bool processEvents(ui::Context *context, bool waitForEvent);
  void beginFrame();
  /** Finish the frame (the renderer composites its layers into the window). */
  void endFrame();
  void presentFrame();
  /** The frame just finished (after endFrame, before presentFrame), as a PNG at `path`. */
  bool saveFrame(const std::string &path) const;

private:
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace admin::ui_host
