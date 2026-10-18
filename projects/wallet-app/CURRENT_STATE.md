# Current state — wallet app

| Capability | Status | Notes |
|------------|--------|-------|
| Design (wl0) | Done | [DESIGN.md](DESIGN.md): a last-resort wallet for everyone's accounts; five promises, threats, reads verified to a pinned genesis hash, evacuation, a key-backup contract; WL001–WL009. The history checked for secrets before going public: none |
| pp-ledger: account proofs, client verification (wl1) | Done (pp-ledger, unreleased) | `ACCOUNT_PROOF` (an account or its absence in the tip block's state root, its Merkle path); `verify::` — `checkGenesis`, `authorityAt`, `checkBlocksUnderTip`, `checkAccountProof`; `pp-client verified-account`; L-SMOKE-PROOF |
| Verified read-only wallet (wl2) | Done | Network cards ([docs/NETWORK_CARD.md](../../docs/NETWORK_CARD.md)) pinned in the vault; `net/WalletChain` (`Verifier`, `checkNodes`); Accounts (proven, or proven absent) and Network (each node verified, one behind flagged) screens |
| Send and receive (wl3) | Done | Operations built from the proven account (keys, balances; the attachment checked by its proven hash); signed with the vault's keys and sent, or saved for the other holders; read back proven |
| Evacuate (wl4) | Done | Replace the keys with a new key made in the wallet, or move everything to another account (a batch). Chain smoke: a send and a key replacement, checked on the chain |
| Keys in and out, offline (wl5) | Done | pp-ledger's key-backup contract (`KEY_BACKUP.md`, `pp-client key-export`/`key-import`); every app's Keys screen saves and imports key backups; offline signing through Review and sign |
| Packages, public repository (wl6) | Done | The repository is public (people-post); a standalone `pp-ledger-wallet` package (.deb with its own assets, AppImage, disk image); no Windows (WL008 superseded); `PP_APPS_RELEASE` builds without build-machine paths; `SHA256SUMS` in releases; [docs/REPRODUCIBLE.md](../../docs/REPRODUCIBLE.md); [docs/guides/wallet.md](../../docs/guides/wallet.md) |
| Account recovery (wl7) | Done | pp-ledger `projects/account-recovery`: Recovery screen (`AccountRecoveryScreen`: set or remove, cancel, recover an account), the announcement on Accounts (proven); the token app's Signers and Overview; `net::makeRecoverySet`, `makeRecoveryCancel`, `makeRecoveryAnnouncement`; chain smoke 6b (set, announced from a second wallet, cancelled); WL010 |
