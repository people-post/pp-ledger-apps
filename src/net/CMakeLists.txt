# apps_net: a role app's link to the chain — reading it, submitting operations, following them in.
# The Signer never links it (AD005).
add_library(apps_net STATIC
    AdminOperations.cpp
    AdminOperations.h
    ChainLink.cpp
    ChainLink.h
    FeeOperations.cpp
    FeeOperations.h
    TokenOperations.cpp
    TokenOperations.h
    WalletChain.cpp
    WalletChain.h
)
target_include_directories(apps_net PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(apps_net PUBLIC apps_core pp::client pp::chain)
pp_apps_apply_warnings(apps_net)

if(PP_APPS_BUILD_TESTS)
  add_subdirectory(tests)
endif()
