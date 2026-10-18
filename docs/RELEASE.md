# Releasing

Push a tag `v<major>.<minor>.<patch>`: `.github/workflows/release.yml` builds the packages and
publishes them as a GitHub release.

| Package | Built on |
|---------|----------|
| `pp-ledger-signer_<v>_amd64.deb`: the Signer and the shared assets, under `/usr` | ubuntu-24.04: needs glibc 2.38+ (the C++ runtime is linked in) |
| `pp-ledger-system_<v>_amd64.deb`: Genesis, Admin, Reserve, Registrar, Fees; depends on `pp-ledger-signer` | Same |
| `pp-ledger-token_<v>_amd64.deb`: the Token app; depends on `pp-ledger-signer` | Same |
| `pp-ledger-wallet_<v>_amd64.deb`: the Wallet, standing alone (its own copy of the assets) | Same |
| `PP_Ledger_<App>-<v>-x86_64.AppImage`, one per app | ubuntu-24.04 (`scripts/linux_appimages.sh`) |
| `pp-ledger-system-<v>-macos-arm64.dmg`: the Signer and the five system apps | macos-14: Apple silicon, macOS 13+ (`scripts/macos_dmg.sh`) |
| `pp-ledger-token-<v>-macos-arm64.dmg`: the Signer and the Token app | Same |
| `pp-ledger-wallet-<v>-macos-arm64.dmg`: the Wallet alone | Same |
| `pp-ledger-wallet-<v>-windows-x64.exe`: the Wallet's installer (NSIS: Start menu and desktop shortcuts, an uninstaller) | windows-2022, MSVC |
| `SHA256SUMS`: every file's SHA-256 | The publish job |

Release builds set `PP_APPS_RELEASE`: no path of the build machine goes into the binaries, so they can
be rebuilt and compared ([REPRODUCIBLE.md](REPRODUCIBLE.md)).

The sets are CMake install components (`cmake/AppsPackaging.cmake`: `signer`, `system`, `token`, `wallet`;
each app names its own in `pp_apps_app`).

A pull request touching packaging builds them too, without publishing.

## macOS signing

The bundles are signed (Developer ID, hardened runtime) and the disk image notarized and stapled
when the repository's Actions secrets are set:

| Secret | What |
|--------|------|
| `APPLE_CERTIFICATE_BASE64` | The Developer ID Application certificate, `.p12`, base64 |
| `APPLE_CERTIFICATE_PASSWORD` | Its export password |
| `APPLE_SIGNING_IDENTITY` | `Developer ID Application: <Org> (<TEAMID>)` |
| `APPLE_TEAM_ID` | The team id |
| `APPLE_NOTARY_KEY_ID`, `APPLE_NOTARY_ISSUER_ID` | The App Store Connect API key for notarization |
| `APPLE_NOTARY_P8_BASE64` | That key's `.p8`, base64 |

Without them the apps are unsigned (macOS asks before opening them). The same certificate and key
as pp-browser's serve: bundle ids are `dev.pp-ledger-apps.<app>`. Local signing:
`packaging/macos/signing.env.example`, `scripts/macos_sign_and_notarize.sh`.

## Icons

`scripts/make_icons.py` draws `assets/icons/<app>.png` (no imaging library needed); the PNGs are
committed.
