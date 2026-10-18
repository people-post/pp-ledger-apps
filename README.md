# pp-ledger-admin

Desktop apps for the people who hold a [pp-ledger](https://github.com/people-post/pp-ledger) chain's
system keys — kick-starting the chain and holding its restore keys, administering it (minting, config
updates), operating its reserve and creating accounts. For non-technical operators: every step is a
form or a file. Linux and macOS.

Work in progress: [projects/admin-apps](projects/admin-apps/DESIGN.md).

## Build

Needs CMake 3.24+ and a C++20 compiler; dependencies (pp-ledger, pp-cpp-common, pp-cpp-crypto,
GoogleTest) are fetched at pinned versions.

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Local checkouts instead of the pins: `-DPP_LEDGER_SOURCE_DIR=../pp-ledger`
(`PP_CPP_COMMON_SOURCE_DIR`, `PP_CPP_CRYPTO_SOURCE_DIR` likewise).

| Piece | What |
|-------|------|
| `src/core` (`admin_core`) | The vault, key cards and the address book, the operation workflow — no network code |
