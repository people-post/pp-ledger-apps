# Current state — admin apps

| Capability | Status | Notes |
|------------|--------|-------|
| Design (ad0) | Done | [DESIGN.md](DESIGN.md): five apps (four role apps, an offline Signer), the operation file, parallel signing, the offline genesis ceremony, the vault, amounts; scope against pp-token-admin; decisions AD001–AD010 |
| Operation file in pp-ledger (ad1) | Done | pp-ledger [#142](https://github.com/people-post/pp-ledger/pull/142): contract `docs/contracts/OPERATION_FILE.md`; library `pp::operation` (network-free: links `pp::chain_types`, `pp::ledger`, `pp::ledger_common` only); builders for transfer, mint, new account; beacon certificates and recoveries made by `pp-client mk-beacon-cert` / `mk-recovery` and wrapped (`op wrap`); `pp-client op show / sign / merge / submit / transfer / mint / new-account / wrap`. The config-update builder (it reads the config in force) comes with the Admin app (ad6) |
| Offline-signed genesis in pp-ledger (ad2) | Done | pp-ledger [#143](https://github.com/people-post/pp-ledger/pull/143): `pp-beacon --plan-genesis` (block 0 as a genesis operation, `moreRecords` for its accounts) → `op sign` per admin → `op merge` → `pp-beacon --init --genesis-op`; block 0 is built by `chain_genesis::records` (`pp::chain`), which the Genesis app (ad8) can call; the smoke network starts this way. A re-genesis is still key-signed on the beacon host |
| Skeleton, shared core and UI (ad3) | No | |
| Signer (ad4) | No | |
| Reserve (ad5) | No | |
| Admin (ad6) | No | |
| Registrar (ad7) | No | |
| Genesis (ad8) | No | |
| Packaging, guides (ad9) | No | |
