# Ongoing projects

Living design and delivery notes for work in progress. `docs/` (once there is one) says what the apps
**are**; a project folder says what we are **changing**, and why.

| Question | Source of truth |
|----------|-----------------|
| What is done / next for a project | That project's **`CURRENT_STATE.md`** only |
| Why we chose X | That project's **`DECISIONS.md`** |
| Ordering of the work | That project's **`PHASES.md`** |

When behaviour ships, promote it into `docs/` in the same PR and mark the decision **superseded by**
that doc.

## Active projects

| Project | Intent |
|---------|--------|
| [admin-apps](admin-apps/DESIGN.md) | Desktop apps (Linux, macOS) for the holders of a pp-ledger chain's system keys: an app per role (Genesis, Admin, Reserve, Registrar) and an offline Signer; operations exchanged as files, signed in parallel |
| [token-app](token-app/DESIGN.md) | The token issuers' app (replacing pp-token-admin): an issuer's overview, issuing and sending its token, its keys; multi-signature, on the admin apps' code, its own package |
| [fee-app](fee-app/DESIGN.md) | The fee account's app: rounds that hand the collected fees on to miners and relays — a period report from the chain (and relays' measures from outside), rules kept across rounds, one batch operation per round, evidence on the chain |
| [wallet-app](wallet-app/DESIGN.md) | A last-resort wallet for everyone's own accounts: reads verified to a pinned genesis hash (no node believed), evacuation from leaked keys, offline signing, a key-backup contract for plugins; Linux, macOS, Windows; the repository goes public |
