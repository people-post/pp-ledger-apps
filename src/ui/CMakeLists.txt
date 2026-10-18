# admin_ui: the window host, the app frame, and the screens every app shares — the vault, keys and
# address book, operation review. No network code (the Signer links it).
add_library(admin_ui STATIC
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
target_include_directories(admin_ui PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(admin_ui PRIVATE PP_ADMIN_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets")
target_link_libraries(admin_ui PUBLIC admin_core pp_ui ${PP_UI_SDL3_TARGET} ${PP_UI_SDL3_IMAGE_TARGET})
target_compile_definitions(admin_ui PUBLIC UI_SDL_VERSION_MAJOR=3)
if(PP_ADMIN_DEV_HOOKS)
  target_compile_definitions(admin_ui PUBLIC PP_ADMIN_DEV_HOOKS=1)
endif()
pp_admin_apply_warnings(admin_ui)
