# people-post/pp-cpp-crypto (pp_crypto, sodium, mldsa_native, mlkem_native): pinned release tag, or
# a local checkout (PP_CPP_CRYPTO_SOURCE_DIR). Keep the pin pp-ledger's.
include(FetchContent)
include(PpFetchPin)

if(TARGET pp_crypto AND TARGET sodium)
  return()
endif()

set(PP_CPP_CRYPTO_SOURCE_DIR "" CACHE PATH "Optional local checkout of pp-cpp-crypto")
set(PP_CPP_CRYPTO_GIT_REPOSITORY "https://github.com/people-post/pp-cpp-crypto.git"
  CACHE STRING "Git remote for pp-cpp-crypto")
pp_fetch_git_tag(PP_CPP_CRYPTO_GIT_TAG "v0.2.0" "Release tag on pp-cpp-crypto main")
set(PP_CRYPTO_BUILD_TESTS OFF CACHE BOOL "Build pp-cpp-crypto tests" FORCE)

if(PP_CPP_CRYPTO_SOURCE_DIR)
  add_subdirectory("${PP_CPP_CRYPTO_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/_deps/pp_cpp_crypto-build" EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(pp_cpp_crypto
    GIT_REPOSITORY ${PP_CPP_CRYPTO_GIT_REPOSITORY}
    GIT_TAG ${PP_CPP_CRYPTO_GIT_TAG}
    ${PP_FETCH_EXCLUDE_FROM_ALL})
  FetchContent_MakeAvailable(pp_cpp_crypto)
endif()
foreach(_target pp_crypto sodium mldsa_native)
  if(NOT TARGET ${_target})
    message(FATAL_ERROR "pp-cpp-crypto did not define target ${_target}")
  endif()
endforeach()
