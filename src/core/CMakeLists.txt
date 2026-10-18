# admin_core: vault, key cards and address book, the operation workflow. No network code: the
# Signer links this and the UI only (DESIGN.md § Shape of the code, AD005).
add_library(admin_core STATIC
    AddressBook.cpp
    Amounts.cpp
    Amounts.h
    AddressBook.h
    AtomicFile.cpp
    AtomicFile.h
    KeyCard.cpp
    KeyCard.h
    Operations.cpp
    Operations.h
    Roles.cpp
    Roles.h
    Vault.cpp
    Vault.h
)
target_include_directories(admin_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(admin_core PUBLIC pp::operation pp_common sodium)
pp_admin_apply_warnings(admin_core)

if(PP_ADMIN_BUILD_TESTS)
  add_subdirectory(tests)
endif()
