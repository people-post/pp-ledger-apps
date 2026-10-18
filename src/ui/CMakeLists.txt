# apps_ui: the window host, the app frame, and the screens every app shares — the vault, keys and
# address book, operation review. No network code (the Signer links it).
add_library(apps_ui STATIC
    IdleLock.cpp
    IdleLock.h
    KeysScreen.cpp
    KeysScreen.h
    Nav.cpp
    Nav.h
    ReviewScreen.cpp
    ReviewScreen.h
    Screen.cpp
    Screen.h
    Session.h
    UiApp.cpp
    UiApp.h
    VaultScreen.cpp
    VaultScreen.h
    WindowHost.cpp
    WindowHost.h
)
target_include_directories(apps_ui PUBLIC ${PROJECT_SOURCE_DIR}/src)
# Development builds find the assets in the source tree; a release only beside the program (UiApp).
if(PP_APPS_RELEASE)
  target_compile_definitions(apps_ui PRIVATE PP_APPS_ASSETS_DIR="assets")
else()
  target_compile_definitions(apps_ui PRIVATE PP_APPS_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets")
endif()
target_link_libraries(apps_ui PUBLIC apps_core pp_ui ${PP_UI_SDL3_TARGET} ${PP_UI_SDL3_IMAGE_TARGET})
target_compile_definitions(apps_ui PUBLIC UI_SDL_VERSION_MAJOR=3)
if(PP_APPS_DEV_HOOKS)
  target_compile_definitions(apps_ui PUBLIC PP_APPS_DEV_HOOKS=1)
endif()
pp_apps_apply_warnings(apps_ui)
