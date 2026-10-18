# people-post/pp-cpp-common (target pp_common): pinned release tag, or a local checkout
# (PP_CPP_COMMON_SOURCE_DIR). Keep the pin pp-ledger's.
include(FetchContent)
include(PpFetchPin)

if(TARGET pp_common)
  return()
endif()

set(PP_CPP_COMMON_SOURCE_DIR "" CACHE PATH "Optional local checkout of pp-cpp-common")
set(PP_CPP_COMMON_GIT_REPOSITORY "https://github.com/people-post/pp-cpp-common.git"
  CACHE STRING "Git remote for pp-cpp-common")
pp_fetch_git_tag(PP_CPP_COMMON_GIT_TAG "v0.3.1" "Release tag on pp-cpp-common main")
set(PP_COMMON_BUILD_TESTS OFF CACHE BOOL "Build pp-cpp-common tests" FORCE)

if(PP_CPP_COMMON_SOURCE_DIR)
  add_subdirectory("${PP_CPP_COMMON_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/_deps/pp_cpp_common-build" EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(pp_cpp_common
    GIT_REPOSITORY ${PP_CPP_COMMON_GIT_REPOSITORY}
    GIT_TAG ${PP_CPP_COMMON_GIT_TAG})
  FetchContent_MakeAvailable(pp_cpp_common)
endif()
if(NOT TARGET pp_common)
  message(FATAL_ERROR "pp-cpp-common did not define target pp_common")
endif()
