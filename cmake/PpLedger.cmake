# people-post/pp-ledger, libraries only (its apps are not built when embedded): pp::operation (the
# operation file, network-free — all the Signer links), pp::client and pp::chain (the role apps).
# Pinned release tag, or a local checkout (PP_LEDGER_SOURCE_DIR).
include(FetchContent)
include(PpFetchPin)

set(PP_LEDGER_SOURCE_DIR "" CACHE PATH "Optional local checkout of pp-ledger")
set(PP_LEDGER_GIT_REPOSITORY "https://github.com/people-post/pp-ledger.git"
  CACHE STRING "Git remote for pp-ledger")
pp_fetch_git_tag(PP_LEDGER_GIT_TAG "release/v1.3.0" "pp-ledger release tag")

if(PP_LEDGER_SOURCE_DIR)
  add_subdirectory("${PP_LEDGER_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/_deps/pp_ledger-build" EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(pp_ledger
    GIT_REPOSITORY ${PP_LEDGER_GIT_REPOSITORY}
    GIT_TAG ${PP_LEDGER_GIT_TAG}
    ${PP_FETCH_EXCLUDE_FROM_ALL})
  FetchContent_MakeAvailable(pp_ledger)
endif()
foreach(_target pp::operation pp::client)
  if(NOT TARGET ${_target})
    message(FATAL_ERROR "pp-ledger did not define target ${_target}")
  endif()
endforeach()
