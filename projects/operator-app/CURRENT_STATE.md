# Current state — operator app

| Capability | Status | Notes |
|------------|--------|-------|
| Design (op0) | Proposed | [DESIGN.md](DESIGN.md): services of three kinds (relay, node, ledger relay), each with its own operator set and certificates; the same six screens for every kind; the certificate operation file; OP001–OP007 |
| Everything else (op1–op6) | Not started | The relay kind first. Release 1's staging and prod relay ceremonies use pp-relay-apis' `relay-operator.mjs`, not this app |

**Waiting on others:**
- pp-browser: the node certificate's format (with its signed node records, N031: after release 1); the relay
  certificate's codec in a shared library (OP003).
- pp-ledger: the ledger relay certificate's format, and `pp-relay operator request` / `install`.

**Open (defaults until decided):**
- The app's name: `pp-ledger-operator` ("PP Operator"), in the `system` package.
- Node and ledger relay certificates: the relay certificate's envelope, each kind under its own signing domain (OP007).
- Expiry for nodes and ledger relays: 90 days, with the Services list warning from 14 days before; relays keep release
  1's "none".
