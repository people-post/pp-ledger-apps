# The apps as installed: a bundle each on macOS (its icon, the shared assets in Resources); a binary,
# desktop entry and icon each on Linux (the assets under share/pp-ledger-apps, and a copy of the wallet's
# own under share/pp-ledger-wallet: its package stands alone). UiApp::assetPath finds the assets either way.

# pp_apps_app(<target> <slug> <title> <comment> <package>): `slug` names the icon
# (assets/icons/<slug>.png) and ids; `title` is what people see ("PP Ledger Signer"); `package` the
# install component it ships in (AppsPackaging.cmake): signer, system, token or wallet.
function(pp_apps_app target slug title comment package)
  set(icon ${PROJECT_SOURCE_DIR}/assets/icons/${slug}.png)
  if(APPLE)
    target_sources(${target} PRIVATE ${icon})
    set_source_files_properties(${icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(${target} PROPERTIES
      MACOSX_BUNDLE TRUE
      OUTPUT_NAME "${title}"
      MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in
      MACOSX_BUNDLE_BUNDLE_NAME "${title}"
      MACOSX_BUNDLE_GUI_IDENTIFIER "dev.pp-ledger-apps.${slug}"
      MACOSX_BUNDLE_ICON_FILE "${slug}.png"
      MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
      MACOSX_BUNDLE_COPYRIGHT "People Post")
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets
              "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/assets"
      VERBATIM)
    install(TARGETS ${target} BUNDLE DESTINATION . COMPONENT ${package})
  else()
    # One less library a distribution must match: the C++ runtime comes with the binary.
    target_link_options(${target} PRIVATE -static-libstdc++ -static-libgcc)
    install(TARGETS ${target} RUNTIME DESTINATION bin COMPONENT ${package})
    set(PP_APP_SLUG ${slug})
    set(PP_APP_TITLE "${title}")
    set(PP_APP_COMMENT "${comment}")
    set(PP_APP_EXEC ${target})
    configure_file(${PROJECT_SOURCE_DIR}/packaging/linux/app.desktop.in
                   ${CMAKE_CURRENT_BINARY_DIR}/pp-ledger-apps-${slug}.desktop @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pp-ledger-apps-${slug}.desktop DESTINATION share/applications
            COMPONENT ${package})
    install(FILES ${icon} DESTINATION share/icons/hicolor/512x512/apps RENAME pp-ledger-apps-${slug}.png
            COMPONENT ${package})
  endif()
endfunction()

# The assets ship with the Signer, which the system and token packages depend on; the wallet's package
# carries its own copy (it stands alone).
if(NOT APPLE)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/ DESTINATION share/pp-ledger-apps/assets COMPONENT signer)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/ DESTINATION share/pp-ledger-wallet/assets COMPONENT wallet)
endif()
