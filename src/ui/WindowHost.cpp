#include "ui/WindowHost.h"

#include <ui/core/Core.h>
#include <ui/dom/Context.h>
#include <ui/platform/Platform_SDL.h>
#include <ui/render/Renderer_GL3.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <vector>

namespace apps::ui_host {

struct WindowHost::State {
  SystemInterface_SDL system;
  RenderInterface_GL3 render;
  SDL_Window *window = nullptr;
  SDL_GLContext gl = nullptr;
  int width = 0;
  int height = 0;
};

WindowHost::WindowHost() = default;

WindowHost::~WindowHost() { close(); }

bool WindowHost::open(const std::string &title, int width, int height) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return false;
  }
  SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  const float scale = std::max(1.0f, SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay()));
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, static_cast<int>(width * scale));
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, static_cast<int>(height * scale));
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
  SDL_Window *window = SDL_CreateWindowWithProperties(props);
  SDL_DestroyProperties(props);
  if (!window) {
    SDL_Log("Cannot create the window: %s", SDL_GetError());
    SDL_Quit();
    return false;
  }
  SDL_GLContext gl = SDL_GL_CreateContext(window);
  if (!gl) {
    SDL_Log("Cannot create an OpenGL 3.3 context: %s", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return false;
  }
  SDL_GL_MakeCurrent(window, gl);
  SDL_GL_SetSwapInterval(1);
  if (!RmlGL3::Initialize()) {
    SDL_Log("Cannot initialize the OpenGL renderer");
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return false;
  }
  state_ = std::make_unique<State>();
  state_->window = window;
  state_->gl = gl;
  SDL_GetWindowSizeInPixels(window, &state_->width, &state_->height);
  state_->system.SetWindow(window);
  state_->render.SetViewport(state_->width, state_->height);
  return true;
}

void WindowHost::close() {
  if (!state_) {
    return;
  }
  SDL_Window *window = state_->window;
  SDL_GLContext gl = state_->gl;
  state_.reset(); // the renderer goes while its context is current
  RmlGL3::Shutdown();
  SDL_GL_DestroyContext(gl);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

ui::SystemInterface *WindowHost::systemInterface() { return &state_->system; }

ui::RenderInterface *WindowHost::renderInterface() { return &state_->render; }

SDL_Window *WindowHost::window() const { return state_ ? state_->window : nullptr; }

float WindowHost::densityRatio() const { return state_ ? SDL_GetWindowDisplayScale(state_->window) : 1.0f; }

bool WindowHost::processEvents(ui::Context *context, bool waitForEvent) {
  SDL_Event ev;
  bool hasEvent = waitForEvent ? SDL_WaitEventTimeout(&ev, 100) : SDL_PollEvent(&ev);
  bool running = true;
  while (hasEvent) {
    switch (ev.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      running = false;
      break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      state_->width = ev.window.data1;
      state_->height = ev.window.data2;
      state_->render.SetViewport(state_->width, state_->height);
      context->SetDimensions({state_->width, state_->height});
      break;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      context->SetDensityIndependentPixelRatio(densityRatio());
      break;
    default:
      break;
    }
    RmlSDL::InputEventHandler(context, state_->window, ev);
    hasEvent = SDL_PollEvent(&ev);
  }
  return running;
}

void WindowHost::beginFrame() {
  state_->render.Clear();
  state_->render.BeginFrame();
}

void WindowHost::endFrame() { state_->render.EndFrame(); }

void WindowHost::presentFrame() { SDL_GL_SwapWindow(state_->window); }

bool WindowHost::saveFrame(const std::string &path) const {
  using ReadPixels = void (*)(int, int, int, int, unsigned, unsigned, void *);
  auto readPixels = reinterpret_cast<ReadPixels>(SDL_GL_GetProcAddress("glReadPixels"));
  if (!readPixels) {
    return false;
  }
  const int w = state_->width;
  const int h = state_->height;
  std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 4);
  constexpr unsigned kRgba = 0x1908;         // GL_RGBA
  constexpr unsigned kUnsignedByte = 0x1401; // GL_UNSIGNED_BYTE
  readPixels(0, 0, w, h, kRgba, kUnsignedByte, pixels.data());
  SDL_Surface *surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
  if (!surface) {
    return false;
  }
  for (int y = 0; y < h; ++y) { // OpenGL rows run bottom up
    std::copy_n(pixels.data() + static_cast<size_t>(h - 1 - y) * w * 4, static_cast<size_t>(w) * 4,
                static_cast<unsigned char *>(surface->pixels) + static_cast<size_t>(y) * surface->pitch);
  }
  const bool saved = IMG_SavePNG(surface, path.c_str());
  SDL_DestroySurface(surface);
  return saved;
}

} // namespace apps::ui_host
