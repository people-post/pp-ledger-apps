# Current state — token app

| Capability | Status | Notes |
|------------|--------|-------|
| Design (tk0) | Done | [DESIGN.md](DESIGN.md): issuers on pp-ledger, the app's screens, what it needs from pp-ledger, its package; TA001–TA006. Fee distribution noted for later |
| Account updates in operation files (tk1) | Done (pp-ledger, unreleased) | Kind `account-update`, `op::replaceKeys`, `pp-client op update-keys`; a `T_USER_UPDATE` keeps the chain's balances; admission lets an issuer send its own token; L-SMOKE-TOKEN |
| Overview, issue, send (tk2) | Done | `net/TokenOperations` (overview, issue, send, renewal fee), `ChainLink::transactions`; `pp-ledger-token`: Overview (the issuer kept in the vault, `token.json`), Issue, Send; chain smoke step 4 |
| Key replacement (tk3) | Done | Signers screen: the keys in force, the vault's and address book's owner keys, key cards; the vault holds owner keys |
| Package, guide (tk4) | Done | Install components `signer`, `system`, `token`: `.deb`s `pp-ledger-signer`, `pp-ledger-system`, `pp-ledger-token` (the latter two depend on the first); disk images `pp-ledger-system` and `pp-ledger-token` (`macos_dmg.sh … system\|token`); AppImage per app; `token.png`. [docs/guides/token.md](../../docs/guides/token.md). Checked locally: the three `.deb`s' contents and depends, the Token app run from the token and signer packages extracted together. pp-token-admin archived 2026-10-18, its README pointing here |
| Symbol (tk5) | Done | Overview shows the issuer's domain — its token's symbol (pp-ledger `projects/domains`) |
