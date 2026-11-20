# apps_core: vault, key cards and address book, the operation workflow. No network code: the
# Signer links this and the UI only (DESIGN.md § Shape of the code, AD005).
add_library(apps_core STATIC
    AddressBook.cpp
    Amounts.cpp
    Amounts.h
    AddressBook.h
    AtomicFile.cpp
    AtomicFile.h
    FeeRounds.cpp
    FeeRounds.h
    KeyCard.cpp
    KeyCard.h
    KeyBackups.cpp
    KeyBackups.h
    Operations.cpp
    Operations.h
    Roles.cpp
    Roles.h
    Vault.cpp
    Vault.h
    WalletBook.cpp
    WalletBook.h
)
target_include_directories(apps_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(apps_core PUBLIC pp::operation pp_common sodium)
pp_apps_apply_warnings(apps_core)

if(PP_APPS_BUILD_TESTS)
  add_subdirectory(tests)
endif()
