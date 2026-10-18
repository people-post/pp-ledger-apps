# pp-ledger-admin

Desktop apps for the people who hold a [pp-ledger](https://github.com/people-post/pp-ledger) chain's
system keys — kick-starting the chain and holding its restore keys, administering it (minting, config
updates), operating its reserve and creating accounts. For non-technical operators: every step is a
form or a file. Linux and macOS.

Work in progress: [projects/admin-apps](projects/admin-apps/DESIGN.md).

## Build

Needs CMake 3.24+ and a C++20 compiler (on Linux, X11 and OpenGL headers:
`libx11-dev libxext-dev libxcursor-dev libxinerama-dev libxi-dev libxrandr-dev libxfixes-dev
libxrender-dev libxss-dev libgl-dev`); dependencies (pp-ledger, pp-cpp-ui with SDL3,
pp-cpp-common, pp-cpp-crypto, GoogleTest) are fetched at pinned versions.

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Local checkouts instead of the pins: `-DPP_LEDGER_SOURCE_DIR=../pp-ledger`
(`PP_CPP_UI_SOURCE_DIR`, `PP_CPP_COMMON_SOURCE_DIR`, `PP_CPP_CRYPTO_SOURCE_DIR` likewise).

Run: `./build/src/apps/signer/pp-admin-signer [operation file]`. The vault lives in
`~/.pp-ledger-admin/vault` (Linux) or `~/Library/Application Support/pp-ledger-admin/vault`
(macOS); `PP_LEDGER_ADMIN_HOME` moves it.

Development builds (`-DPP_ADMIN_DEV_HOOKS=ON`, never a release) add `pp-admin-devfixture` (a vault
and an operation to review) and let the apps run a script (`--script`: click, type, answer file
dialogs, take screenshots); `scripts/ui_smoke.sh` drives the Signer that way under Xvfb (a ctest
when `xvfb-run` is present).

| Piece | What |
|-------|------|
| `src/core` (`admin_core`) | The vault, key cards and the address book, the operation workflow — no network code |
| `src/ui` (`admin_ui`) | The window host (SDL3 + OpenGL 3, pp-cpp-ui), the app frame, the shared screens: vault, keys and people, operation review |
| `src/apps/signer` | `pp-admin-signer`: review and sign operations, offline |
| `assets/` | Views (RML), the theme (RCSS), the font (Lato, SIL Open Font License 1.1: `assets/fonts/NOTICE`) |
