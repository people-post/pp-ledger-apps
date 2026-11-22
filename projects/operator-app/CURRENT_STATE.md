# Current state — operator app

| Capability | Status | Notes |
|------------|--------|-------|
| Design (op0) | Proposed | [DESIGN.md](DESIGN.md): services of three kinds (relay, node, ledger relay), each with its own operator set and certificates; the same six screens for every kind; the certificate operation file; OP001–OP007 |
| op1 (the codec) | Waiting | On pp-browser moving the relay certificate code to its capabilities repository, after its release 1 (OP008). A reference port with the vectors passing: pp-cpp-crypto branch `feat/relay-certificate`, not merged; OP008 lists what it found |
| Everything else (op2–op6) | Not started | The relay kind first, after op1. Release 1's staging and prod relay ceremonies use pp-relay-apis' `relay-operator.mjs`, not this app |

**Waiting on others:**
- pp-browser: the relay certificate code in its capabilities repository (OP008), and the node certificate's format
  (with its signed node records, N031); both after its release 1.
- pp-ledger: the ledger relay certificate's format, and `pp-relay operator request` / `install`.

**Open (defaults until decided):**
- The app's name: `pp-ledger-operator` ("PP Operator"), in the `system` package.
- Node and ledger relay certificates: the relay certificate's envelope, each kind under its own signing domain (OP007).
- Expiry for nodes and ledger relays: 90 days, with the Services list warning from 14 days before; relays keep release
  1's "none".
