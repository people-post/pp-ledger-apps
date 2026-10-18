# Releasing

Push a tag `v<major>.<minor>.<patch>`: `.github/workflows/release.yml` builds the packages and
publishes them as a GitHub release.

| Package | Built on |
|---------|----------|
| `pp-ledger-signer_<v>_amd64.deb`: the Signer and the shared assets, under `/usr` | ubuntu-24.04: needs glibc 2.38+ (the C++ runtime is linked in) |
| `pp-ledger-admin_<v>_amd64.deb`: Genesis, Admin, Reserve, Registrar; depends on `pp-ledger-signer` | Same |
| `pp-ledger-token_<v>_amd64.deb`: the Token app; depends on `pp-ledger-signer` | Same |
| `PP_Ledger_<App>-<v>-x86_64.AppImage`, one per app | ubuntu-24.04 (`scripts/linux_appimages.sh`) |
| `pp-ledger-admin-<v>-macos-arm64.dmg`: the Signer and the four system apps | macos-14: Apple silicon, macOS 13+ (`scripts/macos_dmg.sh`) |
| `pp-ledger-token-<v>-macos-arm64.dmg`: the Signer and the Token app | Same |

The sets are CMake install components (`cmake/AdminPackaging.cmake`: `signer`, `admin`, `token`;
each app names its own in `pp_admin_app`).

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
as pp-browser's serve: bundle ids are `dev.pp-ledger-admin.<app>`. Local signing:
`packaging/macos/signing.env.example`, `scripts/macos_sign_and_notarize.sh`.

## Icons

`scripts/make_icons.py` draws `assets/icons/<app>.png` (no imaging library needed); the PNGs are
committed.
