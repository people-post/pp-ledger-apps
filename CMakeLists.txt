# pp-ledger-admin: desktop apps for a pp-ledger chain's system key holders (Linux, macOS).
cmake_minimum_required(VERSION 3.24)

# The release workflow sets the version from its tag (v1.2.3).
set(PP_ADMIN_VERSION "0.1.0" CACHE STRING "Version of the apps and packages")
project(pp-ledger-admin VERSION ${PP_ADMIN_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PP_ADMIN_BUILD_TESTS "Build the unit tests" ON)
# Development only (never in a release): a fixture tool, and apps unlocking with
# $PP_ADMIN_DEV_PASSPHRASE / opening a screen with --show, for screenshots.
option(PP_ADMIN_DEV_HOOKS "Development hooks (fixture tool, unattended unlock)" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(AdminWarnings)
# pp-cpp-common and pp-cpp-crypto first: pp-ledger reuses them.
include(PpCppCommon)
include(PpCppCrypto)
include(PpLedger)
include(PpCppUi)
if(PP_ADMIN_BUILD_TESTS)
  include(AdminTesting)
  enable_testing()
endif()

include(AdminApps)
add_subdirectory(src)
include(AdminPackaging)
