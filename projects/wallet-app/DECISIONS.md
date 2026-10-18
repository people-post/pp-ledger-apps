# Wallet app — decisions

Rationale for [DESIGN.md](DESIGN.md). Outcomes move to `docs/` when they ship; mark the entry
**superseded by** that doc then.

## WL001 — Built here; the repository goes public

**Date:** 2026-10-19
**Decision:** The wallet is an app of this repository, on its libraries (vault, key files, operation
files, the node link, the UI host, the review screen). The repository becomes public, in the public
organization, the admin apps included.
**Rationale:** The wallet's trust rests on public, readable code; so do the admin apps', whose security
lies in keys and signatures, not in hidden code. One public repository is one set of libraries and one
pin of pp-ledger; reviewers audit what the wallet links, not the rest.
**Alternatives:** A public library repository and a public wallet repository, the admin apps private
(a third repository and versioned releases between them); a public wallet with copied code (drifts).
**Done:** the repository moved to people-post and is public (2026-10-19).

## WL002 — Five promises

**Date:** 2026-10-19
**Decision:** The wallet promises: no dependency on what the person may distrust (no plugins, web
content, automatic updates or telemetry); keys never leave it but as exported; it believes no node;
it can always evacuate; anyone can check it.
**Rationale:** A last resort is worth what its promises are; every later choice is checked against them.

## WL003 — Reads verified to a pinned genesis hash

**Date:** 2026-10-19
**Decision:** The person pins the network's genesis hash (from a network card, checked against a second
source). The wallet verifies what it shows: the authority proof from block 0 gives the beacon key; the
beacon's signed tip, the tip block, its `stateRoot` and the account's Merkle path give the account.
Several nodes are asked and disagreements shown. What cannot be verified is marked so.
**Rationale:** A node that lies about a balance or a transfer is what a last resort must withstand;
pp-ledger's chain already carries the proofs but the account proof's request.
**Alternatives:** Several nodes' agreement only (a majority of lying nodes wins; nothing proven).
**As built:** pp-ledger's `ACCOUNT_PROOF` and `verify::`; the pin is a network card (docs/NETWORK_CARD.md). Each node's tip is verified, and one far behind the others flagged: a node can show an older state, not a false one.

## WL004 — Evacuate is the wallet's first job

**Date:** 2026-10-19
**Decision:** A guided flow replaces an account's keys with keys made in the wallet, or creates a new
account and moves every balance to it, then verifies the result.
**Rationale:** A person turns to the last resort when a key may have leaked; moving the funds out of
its reach, before anything else, is what they need.

## WL005 — A key-backup contract for plugins

**Date:** 2026-10-19
**Decision:** The wallet's key-backup format is written as a contract in pp-ledger (beside the operation
file); wallet plugins must be able to export to it.
**Rationale:** The last resort is useless if keys cannot reach it.
**Alternatives:** Import each plugin's own format (as many formats as plugins, some never documented).
**As built:** pp-ledger `docs/contracts/KEY_BACKUP.md` — Argon2id and XChaCha20-Poly1305 (libsodium, in browsers too), the public key in the clear and bound to the ciphertext.

## WL006 — The wallet signs offline itself

**Date:** 2026-10-19
**Decision:** With no network set, the wallet reviews and signs operation files.
**Rationale:** Keys on a machine that never connects is the strongest setup a person can have; one app
for both is simpler than a second one to install.

## WL007 — Reproducible, checkable releases

**Date:** 2026-10-19
**Decision:** Release builds are reproducible; releases publish checksums and how to rebuild; packages
are signed where the platform signs.
**Rationale:** Public source proves little if nobody can tie a download to it.
**As built:** `PP_APPS_RELEASE` (no build-machine path in the binaries), `SHA256SUMS` in every release, docs/REPRODUCIBLE.md. The binaries are what is reproducible; package containers and macOS signatures carry their own timestamps.

## WL008 — Windows too

**Superseded (2026-10-19):** no Windows — the owner's decision. Linux and macOS only, as the other apps;
the Windows build and installer were removed before they shipped (the portable arithmetic and date
code they brought stays: it is tested and simpler).

**Date:** 2026-10-19
**Decision:** The wallet ships for Windows as well as Linux and macOS.
**Rationale:** Everyone's wallet; most people use Windows. pp-ledger builds on Windows already; the UI
host and packaging are the work.
**As built:** the code made portable (no 128-bit integers, no timegm, Windows file writes, %APPDATA%), icon and version resources, an NSIS installer of the wallet, the build in CI on windows-2022 for every app. Proven only by that CI: no Windows machine here.

## WL009 — The same strict vault

**Date:** 2026-10-19
**Decision:** The vault as the other apps keep it: Argon2id at libsodium's sensitive limits, a
passphrase, the 10-minute lock. To revisit if people find unlocking too slow.
**Rationale:** The wallet holds the person's funds; a stolen computer must not give them up.

## WL010 — A way back, in the wallet

**Date:** 2026-10-19
**Decision:** The wallet sets an account's recovery set (pp-ledger `projects/account-recovery`: keys
from key cards, a delay in days), warns of a recovery announced — proven, on Accounts — and cancels
it; and announces a recovery for whoever holds an account's recovery keys. The token app sets and
cancels its issuer's; its recovery-key holders use the wallet.
**Rationale:** The last resort is where lost keys are met; the owner's veto only works if the owner
sees the announcement without trusting a node, and the wallet's reads are proven.
