# Wallet app — phases

Ordering only; status lives in [CURRENT_STATE.md](CURRENT_STATE.md). Design: [DESIGN.md](DESIGN.md).

- [ ] **wl0** Design: the promises, threats, verified reads, evacuation, keys in and out (this folder)
- [ ] **wl1** pp-ledger: `ACCOUNT_PROOF`; client verification — authority proof, signed tip, block, account proof
- [ ] **wl2** A verified read-only wallet: the network card and pinned genesis hash, several nodes, accounts verified
- [ ] **wl3** Send and receive: single- and multi-signature accounts
- [ ] **wl4** Evacuate: replace keys, or a new account with everything moved; verified after
- [ ] **wl5** Keys in and out: the key-backup contract (pp-ledger), import and export; offline signing
- [ ] **wl6** Packages: its own, Linux, macOS and Windows; reproducible builds, checksums; the repository public
