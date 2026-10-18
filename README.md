# pp-ledger-apps

Desktop apps for the people who hold a [pp-ledger](https://github.com/people-post/pp-ledger) chain's
system keys — kick-starting the chain and holding its restore keys, administering it (minting, config
updates), operating its reserve and creating accounts — for token issuers' operators and the fee
account's holders, and **everyone's last-resort wallet**: accounts proven from a pinned network, believing
no node, and evacuated from leaked keys. For non-technical people: every step is a form or a file. Linux
and macOS.

Guides for key holders: [docs/guides](docs/guides/getting-started.md). Packages: the repository's
releases (`.deb`s and AppImages for Linux, disk images for macOS, with checksums; [docs/RELEASE.md](docs/RELEASE.md)).
Design: [projects/admin-apps](projects/admin-apps/DESIGN.md), [projects/token-app](projects/token-app/DESIGN.md),
[projects/fee-app](projects/fee-app/DESIGN.md), [projects/wallet-app](projects/wallet-app/DESIGN.md).

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

Run: `./build/src/apps/signer/pp-ledger-signer [operation file]`. The vault lives in
`~/.pp-ledger-apps/vault` (Linux) or `~/Library/Application Support/pp-ledger-apps/vault`
(macOS); `PP_LEDGER_APPS_HOME` moves it.

Development builds (`-DPP_APPS_DEV_HOOKS=ON`, never a release) add `pp-ledger-devfixture` (a vault
and an operation to review) and let the apps run a script (`--script`: click, type, answer file
dialogs, check what is shown, take screenshots); `scripts/ui_smoke.sh` drives the Signer that way
under Xvfb (a ctest when `xvfb-run` is present). `scripts/chain_smoke.sh <build> <pp-ledger checkout>`
drives the role apps against pp-ledger's local smoke network (a built pp-ledger with its apps), each
operation made from the chain, signed with the smoke keys, submitted, and checked on chain: a
reserve → registrar transfer; a config update raising the mint cap, a mint, an admin-set rotation;
an issued account for an owner's key, funded by the registrar; a recovery naming a new admin set.
`scripts/genesis_smoke.sh <build> <pp-ledger checkout>` runs the offline genesis ceremony: the
Genesis app plans block 0 from key cards, its admin keys sign it, and `pp-beacon --init --genesis-op`
writes block 0 from it.

| Piece | What |
|-------|------|
| `src/core` (`apps_core`) | The vault, key cards and the address book, the operation workflow, the fee rounds (rules, payouts, round files) — no network code |
| `src/net` (`apps_net`) | The link to a node: chain info and the config in force (`CONFIG_GET`), accounts and their signers, the minimum fee, submitting, finding a transaction in a block; the admin set's, a token issuer's and the fee account's operations built and checked by pp-ledger's rules (`AdminOperations`, `TokenOperations`, `FeeOperations`) |
| `src/ui` (`apps_ui`) | The window host (SDL3 + OpenGL 3, pp-cpp-ui), the app frame, the shared screens: vault, keys and people, operation review |
| `src/online` (`apps_online`) | What role apps add: the node setting (kept in the vault), chain work off the UI thread, submitting and following an operation, the transfer form |
| `src/apps/signer` | `pp-ledger-signer`: review, sign and merge operations (genesis plans included), offline; locks itself after 10 minutes without use |
| `src/apps/admin` | `pp-ledger-admin`: minting to the reserve, config updates (the changes shown against the chain), rotating the admin set, beacon certificates |
| `src/apps/genesis` | `pp-ledger-genesis`: the genesis plan (block 0 from everyone's key cards, pp-ledger's defaults, signed offline for `pp-beacon --init --genesis-op`) and recovery announcements |
| `src/apps/registrar` | `pp-ledger-registrar`: issued accounts (2^20 … 2^30) for their owners' keys, funded by the registrar |
| `src/apps/reserve` | `pp-ledger-reserve`: transfers out of the reserve — made from the chain, signed, merged, submitted, followed into a block |
| `src/apps/fees` | `pp-ledger-fees`: fee rounds — a period read from the chain, the relays' measures, the rules, the payouts as one batch from the fee account; round files checked by their digest |
| `src/apps/token` | `pp-ledger-token`: a token issuer's overview, issuing its token, sending, replacing its signers |
| `src/apps/wallet` | `pp-ledger-wallet`: everyone's last resort — the network pinned from a card, accounts proven (no node believed), send, receive, evacuate, key backups |
| `assets/` | Views (RML), the theme (RCSS), the font (Lato, SIL Open Font License 1.1: `assets/fonts/NOTICE`) |
