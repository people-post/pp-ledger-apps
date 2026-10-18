# admin_net: a role app's link to the chain — reading it, submitting operations, following them in.
# The Signer never links it (AD005).
add_library(admin_net STATIC
    AdminOperations.cpp
    AdminOperations.h
    ChainLink.cpp
    ChainLink.h
)
target_include_directories(admin_net PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(admin_net PUBLIC admin_core pp::client pp::chain)
pp_admin_apply_warnings(admin_net)

if(PP_ADMIN_BUILD_TESTS)
  add_subdirectory(tests)
endif()
