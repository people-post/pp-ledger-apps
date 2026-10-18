# Admin apps — phases

Ordering only; status lives in [CURRENT_STATE.md](CURRENT_STATE.md). Design: [DESIGN.md](DESIGN.md).

## Foundations (pp-ledger)

- [x] **ad0** Design: roles, apps, operation file, ceremony, vault (this folder)
- [x] **ad1** pp-ledger: the operation file — contract (`docs/contracts/OPERATION_FILE.md`) and
      library: build each kind (transfer, mint, config update, beacon certificate, recovery, new
      account, genesis), recompute its summary, verify signatures against the signer set, merge
      copies; `pp-client` commands to show, sign, merge and submit operation files (AD004)
- [x] **ad2** pp-ledger: `pp-beacon --init` from a signed genesis operation (AD003); the smoke
      fixture starts from one

## Apps

- [x] **ad3** This repo's skeleton: CMake (pp-ledger, pp-cpp-ui, pp-cpp-crypto), `apps_core`
      (vault, key cards / address book), `apps_ui` (unlock, keys, operation review), CI on Linux and
      macOS
  - [x] **ad3a** CMake (pp-ledger, pp-cpp-common, pp-cpp-crypto, GoogleTest), `apps_core`, CI
  - [x] **ad3b** pp-cpp-ui, the window host, `apps_ui`
- [x] **ad4** Signer (offline): open an operation, review it, sign with a vault key, merge copies,
      save — the first app, used by every role (review and signing came with ad3b; ad4 adds merging,
      genesis operations' many records, packaging-ready polish)
- [x] **ad5** Reserve: transfers; `apps_net` (read chain state, submit, follow inclusion)
- [x] **ad6** Admin: minting to the reserve, config updates (shown as a change against the config in
      force), beacon certificates, admin key rotation
- [x] **ad7** Registrar: creating and funding issued accounts
- [x] **ad8** Genesis: key cards, the genesis plan, recovery operations
- [x] **ad9** Packaging (AppImage / `.deb`, notarized `.dmg`) and operator guides; promote to `docs/`
- [x] **ad10** Domains: the registrar's (at creation, or later); the genesis plan's system names
