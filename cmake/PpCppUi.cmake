# people-post/pp-cpp-ui (ui:: engine, SDL3 + OpenGL 3 backend): pinned release tag, or a local
# checkout (PP_CPP_UI_SOURCE_DIR). Keep the pin pp-browser's.
include(FetchContent)
include(PpFetchPin)

set(PP_CPP_UI_SOURCE_DIR "" CACHE PATH "Optional local checkout of pp-cpp-ui")
set(PP_CPP_UI_GIT_REPOSITORY "https://github.com/people-post/pp-cpp-ui.git"
  CACHE STRING "Git remote for pp-cpp-ui")
pp_fetch_git_tag(PP_CPP_UI_GIT_TAG "v0.4.0" "Release tag on pp-cpp-ui main")

set(PP_UI_BUILD_TESTS OFF CACHE BOOL "Build pp-cpp-ui standalone tests" FORCE)
set(UI_TESTS OFF CACHE BOOL "Build UI engine unit tests" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_REQUIRE_BROTLI OFF CACHE BOOL "" FORCE)

if(PP_CPP_UI_SOURCE_DIR)
  add_subdirectory("${PP_CPP_UI_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/_deps/pp_cpp_ui-build" EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(pp_cpp_ui
    GIT_REPOSITORY ${PP_CPP_UI_GIT_REPOSITORY}
    GIT_TAG ${PP_CPP_UI_GIT_TAG})
  FetchContent_MakeAvailable(pp_cpp_ui)
endif()

if(NOT TARGET pp_ui OR NOT TARGET pp_ui_backend OR NOT TARGET ui::core)
  message(FATAL_ERROR "pp-cpp-ui did not define pp_ui / pp_ui_backend / ui::core")
endif()
if(NOT PP_UI_SDL3_TARGET OR NOT PP_UI_SDL3_IMAGE_TARGET)
  message(FATAL_ERROR "pp-cpp-ui did not export PP_UI_SDL3_TARGET / PP_UI_SDL3_IMAGE_TARGET")
endif()
