# Operator app — decisions

Rationale for [DESIGN.md](DESIGN.md). Outcomes move to `docs/` when they ship; mark the entry
**superseded by** that doc then.

## OP001 — The operator app is built here

**Date:** 2026-11-22
**Status:** Proposed
**Decision:** The app for certifying services (relays, pp-nodes, ledger relays) is an app in this repository, on its
code: the vault, key cards and the address book, the Signer, the shared Review screen.
**Rationale:** The people who hold a service's operator keys often hold the chain's keys too, and the work is the same:
keys held offline in an encrypted vault, a threshold of holders signing their own copies, a review that shows what is
signed. One vault and one Signer is one thing to learn and to audit.
**Alternatives:** Keep pp-relay-apis' `relay-operator.mjs` as the only tool (a checkout, Node.js and JSON files: not
for the holders this repository serves; seeds in plain files). An app in pp-relay-apis or pp-browser (a second vault and
a second Signer).

## OP002 — A certificate operation file beside pp-ledger's

**Date:** 2026-11-22
**Status:** Proposed
**Decision:** Certificates and operator-set changes travel in their own file format,
`pp-service-certificate-operation`, with the operation file's shape: a kind, the exact bytes signed, the signers and
their threshold, signatures collected in any order, notes. The Signer opens both.
**Rationale:** A certificate is not a chain transaction: its signed bytes are its own domain's, with no network id, and
it has no validity window before signing. A sibling format keeps pp-ledger's contract about the chain, while the Signer's
open, review, sign and merge stay one path.
**Alternatives:** A new kind in pp-ledger's operation file (it would carry a signing message that is not the chain's).
`relay-operator.mjs`' files as they are (one JSON file per step, one per signature; no shared shape with the Signer's).

## OP003 — The relay certificate's codec from a shared library

**Date:** 2026-11-22
**Status:** Proposed
**Decision:** The C++ codec for the operator set, the relay id and the relay certificate moves out of pp-browser
(`capabilities/net/src/lib/net/RelayCertificate.*`, `capabilities/base/src/foundation/crypto/RelayId.*`) into a library
both repositories pin (pp-cpp-crypto, or pp-cpp-common beside `ExtArea`), with pp-browser's frozen vectors. This
repository pins the library release when op1 starts; pp-browser moves to it after release 1.
**Rationale:** The app must write exactly the bytes the relay and the app read. One codec, tested against the frozen
vectors that pp-relay-apis' TypeScript codec also passes, cannot disagree with them; a copy could.
**Alternatives:** Copy the code here (two C++ codecs to keep equal). Call `relay-operator.mjs` from the app (Node.js
at runtime on an offline machine).

## OP004 — One key per holder per service

**Date:** 2026-11-22
**Status:** Proposed
**Decision:** A vault key for a service has the role `Operator` and names its service. The app refuses to add a key to
a second service's set, and refuses a host key that is also an operator key of the set.
**Rationale:** pp-relay-apis already asks for one key per environment. A key in two sets makes a leak of one a leak of
both, and a review that names the service a key signs for ("Operator of Org relay — prod") is only true if the key
belongs to one.
**Alternatives:** One operator key per person for all their services (fewer keys and fewer backups; one leak moves every
service the person signs for).

## OP005 — The signing log is in the vault

**Date:** 2026-11-22
**Status:** Proposed
**Decision:** Each signature a holder makes is logged in their vault: the service, the kind, the sequence (or the set
generation), the body's digest. Signing refuses a second body at a sequence already signed for that service, and says
which body was signed there.
**Rationale:** Clients keep the first certificate at a sequence and refuse a second (SIGNED_FORMATS § 3.2): two bodies
at one sequence is a conflict that cannot be undone. `relay-operator.mjs` keeps this log beside the key file. In the
vault it is encrypted and backed up with the key, and goes with it to a new machine.
**Alternatives:** The log in a plain file beside the vault (lost or left behind when the vault moves). The coordinator's
log only (two coordinators would not see each other's bodies).

## OP006 — Publishing is a checklist

**Date:** 2026-11-22
**Status:** Proposed
**Decision:** Publish lists what goes where for the service's kind (a relay's secret, pp-browser's `.cert` file and
relay id variable, the relay link; a node's or ledger relay's install file), offers each as text to copy or a file to
save, and checks each off as the coordinator confirms it. It changes nothing outside the app.
**Rationale:** The places a certificate goes are owned by other people and other access (cloud secrets, CI variables, a
host). Missing one is the error pp-browser's `check_relay_ids.sh` now catches at build time for the `.cert` file and
the relay id; the checklist makes the rest visible before then.
**Alternatives:** Write to those places from the app (it would need cloud and repository credentials on a signing
machine).

## OP007 — Kinds the app does not define

**Date:** 2026-11-22
**Status:** Proposed
**Decision:** The node certificate and the ledger relay certificate are specified by pp-browser and pp-ledger. The app
adds a kind when its format and frozen vectors exist. Until then it has the relay kind only. The default asked of both
owners is the relay certificate's envelope under the kind's own signing domain, so decode, verify and Review stay one
code path.
**Rationale:** A format belongs to the code that reads it. The app needs only a body to decode, a change to show, and a
verifier.
**Alternatives:** The app defines a generic service certificate for all three (a format that neither pp-node nor
pp-ledger reads).
