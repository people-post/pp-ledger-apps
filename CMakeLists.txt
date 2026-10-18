# pp-ledger-apps: desktop apps for a pp-ledger chain's system key holders (Linux, macOS).
cmake_minimum_required(VERSION 3.24)

# The release workflow sets the version from its tag (v1.2.3).
set(PP_APPS_VERSION "0.1.0" CACHE STRING "Version of the apps and packages")
project(pp-ledger-apps VERSION ${PP_APPS_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PP_APPS_BUILD_TESTS "Build the unit tests" ON)
# Development only (never in a release): a fixture tool, and apps unlocking with
# $PP_APPS_DEV_PASSPHRASE / opening a screen with --show, for screenshots.
option(PP_APPS_DEV_HOOKS "Development hooks (fixture tool, unattended unlock)" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(AppsWarnings)
# pp-cpp-common and pp-cpp-crypto first: pp-ledger reuses them.
include(PpCppCommon)
include(PpCppCrypto)
include(PpLedger)
include(PpCppUi)
if(PP_APPS_BUILD_TESTS)
  include(AppsTesting)
  enable_testing()
endif()

include(AppsApps)
add_subdirectory(src)
include(AppsPackaging)
