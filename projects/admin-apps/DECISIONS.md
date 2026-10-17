# Admin apps — decisions

Rationale for [DESIGN.md](DESIGN.md). Outcomes move to `docs/` when they ship; mark the entry
**superseded by** that doc then.

## AD001 — An app per role, plus one Signer app; shared code, one build

**Date:** 2026-10-17
**Decision:** Four role apps (Genesis, Admin, Reserve, Registrar) create and submit operations; one
Signer app reviews, signs and merges them, for every key holder. One repo and one build make the five
executables from shared libraries (core, network, UI).
**Rationale:** Each holder sees only what their role may start, and the screen they use most —
reviewing and signing — is the same for everyone. Shared code keeps five apps the cost of one plus
their forms.
**Alternatives:** One app showing the roles of the keys in its vault (fewer installs; every holder
sees an app sized for all roles). An app per role that also signs (signing would differ between apps,
and need the network present).

## AD002 — Signatures are collected in parallel and merged

**Date:** 2026-10-17
**Decision:** Every signer signs their own copy of an operation; any app merges copies of the same
transaction into one.
**Rationale:** Nobody waits for the person before them, and one absent holder does not block the
rest until the quorum is short — what matters when a transaction expires within a day.
**Alternatives:** A fixed chain of signers passing one file (order to keep, and each delay adds up).

## AD003 — The genesis is signed offline; the beacon starts from the signed file

**Date:** 2026-10-17
**Decision:** pp-ledger gains `pp-beacon --init` from a genesis operation signed elsewhere, checked
and written as block 0. The ceremony (key cards → plan → signatures → start) runs in the apps.
**Rationale:** Today the beacon host needs two genesis private keys at init — the most valuable keys
of the chain on an online machine, once. Signing offline keeps every system key in a vault that need
never be online.
**Alternatives:** Keep `--init` signing with key files (simplest; the exposure stays).

## AD004 — The operation file is a pp-ledger contract, with its library

**Date:** 2026-10-17
**Decision:** The operation file's format, the builders of each kind of operation, verification and
merging live in pp-ledger (a contract doc and a library); the apps link it, and `pp-client` reads,
signs and submits the same files.
**Rationale:** Transactions are then built by the chain's own code — no second implementation of the
wire format to drift — and engineers and operators exchange the same files.
**Alternatives:** The format in this repo (pp-client would not speak it; two places building
transactions).

## AD005 — The Signer never touches the network

**Date:** 2026-10-17
**Decision:** The Signer links no network code; everything it needs is in the file and the vault.
**Rationale:** A machine holding system keys need never be online, and the build proves the app
cannot be (no link to `pp::client`).
**Alternatives:** A Signer that checks the chain before signing (more to show; keys on an online
machine).

## AD006 — One vault per person per machine, shared by the apps

**Date:** 2026-10-17
**Decision:** The apps share one vault per machine: keys labelled with their role, a passphrase always
(Argon2id at libsodium's sensitive limits), XChaCha20-Poly1305 per key file, an encrypted backup
export. pp-browser's at-rest design, without its default PIN.
**Rationale:** A person holding two roles keeps one passphrase and one backup. The keys outweigh a
chat profile's: no weak default, and a backup the app asks for.
**Alternatives:** A vault per app (a passphrase and a backup per role held).

## AD007 — The meaning of an operation is recomputed from its bytes

**Date:** 2026-10-17
**Decision:** What an app shows of an operation is derived from the transaction bytes being signed;
descriptions in the file are shown as the creator's notes, never as its meaning.
**Rationale:** A non-technical signer cannot read bytes; they must be able to trust the summary, and
only a summary the app computes is safe from a tampered file.
**Alternatives:** A description signed alongside (a second thing to keep in step with the bytes).
