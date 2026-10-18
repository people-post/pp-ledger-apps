# GoogleTest (pinned release) and a helper registering a test executable.
include(FetchContent)
include(PpFetchPin)

if(NOT TARGET gtest)
  pp_fetch_git_tag(PP_ADMIN_GTEST_GIT_TAG "v1.15.2" "GoogleTest release tag")
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG ${PP_ADMIN_GTEST_GIT_TAG})
  FetchContent_MakeAvailable(googletest)
endif()
include(GoogleTest)

# pp_admin_add_test(<name> <sources...> LIBS <libs...>)
function(pp_admin_add_test name)
  cmake_parse_arguments(ARG "" "" "LIBS" ${ARGN})
  add_executable(${name} ${ARG_UNPARSED_ARGUMENTS})
  target_link_libraries(${name} PRIVATE ${ARG_LIBS} GTest::gtest GTest::gtest_main)
  pp_admin_apply_warnings(${name})
  gtest_discover_tests(${name} DISCOVERY_MODE PRE_TEST)
endfunction()
