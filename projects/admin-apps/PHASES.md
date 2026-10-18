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

- [ ] **ad3** This repo's skeleton: CMake (pp-ledger, pp-cpp-ui, pp-cpp-crypto), `admin_core`
      (vault, key cards / address book), `admin_ui` (unlock, keys, operation review), CI on Linux and
      macOS
  - [ ] **ad3a** CMake (pp-ledger, pp-cpp-common, pp-cpp-crypto, GoogleTest), `admin_core`, CI
  - [ ] **ad3b** pp-cpp-ui, the window host, `admin_ui`
- [ ] **ad4** Signer (offline): open an operation, review it, sign with a vault key, merge copies,
      save — the first app, used by every role
- [ ] **ad5** Reserve: transfers; `admin_net` (read chain state, submit, follow inclusion)
- [ ] **ad6** Admin: minting to the reserve, config updates (shown as a change against the config in
      force), beacon certificates, admin key rotation
- [ ] **ad7** Registrar: creating and funding issued accounts
- [ ] **ad8** Genesis: key cards, the genesis plan, recovery operations
- [ ] **ad9** Packaging (AppImage / `.deb`, notarized `.dmg`) and operator guides; promote to `docs/`
