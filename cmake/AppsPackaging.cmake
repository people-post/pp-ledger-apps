# Packages (cpack; projects/token-app TA002). Three install components:
#   signer — the Signer and the shared assets;
#   system — Genesis, Admin, Reserve, Registrar, Fees: the chain's system key holders;
#   token  — the Token app: token issuers' operators.
#   wallet — the Wallet: everyone's, standing alone (projects/wallet-app).
# Linux: a .deb per component (pp-ledger-signer, pp-ledger-system, pp-ledger-token, pp-ledger-wallet);
# system and token depend on pp-ledger-signer. macOS: the release's disk images are made per set by
# scripts/macos_dmg.sh; `cpack` here makes one of every bundle (local use).
set(CPACK_PACKAGE_NAME "pp-ledger-apps")
set(CPACK_PACKAGE_VENDOR "People Post")
set(CPACK_PACKAGE_CONTACT "pp-ledger-apps maintainers <noreply@github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Desktop apps for the key holders of a pp-ledger chain")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_COMPONENTS_ALL signer system token wallet)
set(CPACK_STRIP_FILES ON)
if(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "PP Ledger Apps")
  set(CPACK_DMG_FORMAT "UDZO")
  set(CPACK_PACKAGE_FILE_NAME "pp-ledger-apps-${PROJECT_VERSION}-macos-${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
else()
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  set(CPACK_DEBIAN_SIGNER_PACKAGE_NAME "pp-ledger-signer")
  set(CPACK_DEBIAN_SYSTEM_PACKAGE_NAME "pp-ledger-system")
  set(CPACK_DEBIAN_TOKEN_PACKAGE_NAME "pp-ledger-token")
  set(CPACK_DEBIAN_WALLET_PACKAGE_NAME "pp-ledger-wallet")
  set(CPACK_COMPONENT_SIGNER_DESCRIPTION "PP Ledger Signer: review and sign pp-ledger operations, offline")
  set(CPACK_COMPONENT_SYSTEM_DESCRIPTION
      "PP Ledger Genesis, Admin, Reserve, Registrar and Fees: the apps of a pp-ledger chain's system key holders")
  set(CPACK_COMPONENT_TOKEN_DESCRIPTION "PP Ledger Token: issue and manage a pp-ledger token")
  set(CPACK_COMPONENT_WALLET_DESCRIPTION "PP Ledger Wallet: your accounts, proven; move them when keys may have leaked")
  set(CPACK_COMPONENT_SYSTEM_DEPENDS signer)
  set(CPACK_COMPONENT_TOKEN_DEPENDS signer)
  set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)
  # SDL loads the display and GL libraries at run time: name them; the rest dpkg-shlibdeps finds.
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1, libx11-6, libxext6, libxcursor1, libxi6, libxrandr2")
endif()
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
include(CPack)
