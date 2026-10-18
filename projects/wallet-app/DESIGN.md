# Wallet app — design

**Status:** built (wl0–wl6; Windows awaits its first CI build). Phases: [PHASES.md](PHASES.md) · decisions: [DECISIONS.md](DECISIONS.md) ·
status today: [CURRENT_STATE.md](CURRENT_STATE.md).

A desktop wallet for **everyone's own accounts** on pp-ledger — the **last resort**: what a person
turns to when they no longer trust the wallet they use every day (a plugin in pp-browser, pp-browser
itself) — to see what they hold, to move it, and above all to **get their funds away from keys that may
have leaked**. It is the apps' eighth app, on their code (WL001); this repository becomes public so
anyone can read what it does (WL001).

## What it promises (WL002)

A last resort is worth exactly what its promises are. The wallet's, written first because the rest
follows from them:

1. **It depends on nothing the person may distrust.** No plugins, no pp-browser code, no web content;
   no automatic updates; no telemetry; it talks to nothing but pp-ledger nodes the person chose.
2. **Keys never leave it** but as the person exports them (an encrypted backup, a key file), and it can
   sign with no network at all.
3. **It believes no node.** What it shows as the chain's — a balance, a transaction in a block — it has
   checked back to the network's genesis hash, which the person pins (WL003). What it cannot check, it
   says so.
4. **It can always evacuate:** replace an account's keys, or move everything to a new account, in one
   guided step (WL004).
5. **Anyone can check it:** public source, reproducible builds, published checksums (WL007).

## What it defends against

| Threat | Defence |
|--------|---------|
| A wallet plugin (or pp-browser) that leaks keys or signs what the person did not mean | The wallet holds its own copy of the keys and moves the funds to keys the plugin never saw (evacuate) |
| A node that lies — a balance, a transfer "in a block", the network itself | Reads verified to the pinned genesis hash: the authority proof, the beacon's signed tip, the block, the account's Merkle proof (WL003) |
| A stolen computer | The vault: Argon2id at libsodium's sensitive limits, a passphrase, the 10-minute lock (WL009) |
| A tampered download | Reproducible builds, published checksums, signed packages (WL007) |
| **Not:** a compromised operating system or hardware | Out of reach of any app; the offline mode on another machine narrows it (WL006) |

## Trusting the chain without trusting a node (WL003)

pp-ledger already gives a node's word a chain of proof back to block 0
([pp-ledger BEACON_AUTHORITY § Trust chain](https://github.com/people-post/pp-ledger/blob/develop/docs/contracts/BEACON_AUTHORITY.md)):

```text
the genesis hash the person pinned
 └─ block 0 (its hash) ── AUTHORITY_PROOF ──► the beacon key in force
     └─ the beacon's signed tip ──► the tip block (its hash) ──► its stateRoot
         └─ the account's Merkle path ──► the account: balances, keys, threshold
```

- **The anchor** is the genesis hash, pinned at first run from a **network card** — a small file the
  network's operators publish (network id, genesis hash, a few node addresses) — checked by the person
  against a second source (the operators' site, a printed hash, someone they trust).
- **The beacon key** comes from replaying `AUTHORITY_PROOF` from block 0: every certificate and recovery
  that changed it, each checked as nodes check them.
- **The tip** is the beacon's signed tip (`STATUS`), checked with that key; its block's header hashes
  to the signed hash, and carries the `stateRoot`.
- **The account**: `stateRoot` is a sparse Merkle tree over account ids (depth 64,
  [WIRE_SCHEMA § Commitments](https://github.com/people-post/pp-ledger/blob/develop/docs/contracts/WIRE_SCHEMA.md)).
  A node proves an account by its leaf and the path's siblings; the wallet recomputes the root. pp-ledger
  needs to serve that proof (`ACCOUNT_PROOF`, wl1); the verification lives in pp-ledger too, public and
  reusable — by pp-browser's own wallet plugins as well.
- **A transaction in a block** is checked the same way: the block's hash links to the signed tip
  (`previousHash`), and the record is in it.

Several nodes are asked, and any disagreement shown: verification makes a lying node useless; asking
several makes a silent one harmless.

## What it does

| Screen | What |
|--------|------|
| **Accounts** | The person's accounts (by number), each verified: native balance, tokens, keys and threshold, the renewal fee against the balance; its latest transactions |
| **Send** | Native or token transfers; a multi-signature account's as an operation file for its other holders |
| **Receive** | The account number (and later a QR code) |
| **Evacuate** | For an account whose keys may have leaked: replace its keys with new ones made here (`T_USER_UPDATE`, signed by the keys in force), or create a new account and move everything to it — prepared, reviewed and submitted in one flow, then checked |
| **Review and sign** | Operation files, as the Signer does: the wallet signs offline too (WL006) |
| **Keys**, **Network** | The vault, key import and export (WL005); the network card, its nodes and what they agree on |

## Keys in and out (WL005)

A last resort is useless if the keys cannot reach it. The wallet imports pp-client key files and its
own encrypted backups; it exports both. **Its key-backup format is documented as a contract** (in
pp-ledger, beside the operation file) that wallet plugins must be able to export to: the person can
always take their keys from a plugin into the wallet. Writing that contract, and asking plugin authors to
honour it, is part of this project.

## Offline (WL006)

With no network set, the wallet is a signer: it reviews and signs operation files made elsewhere — by
its own online copy on another machine, or by pp-client. A careful person keeps their keys on a machine
that never connects, and carries files.

## Packages (WL007, WL008)

`PP Ledger Wallet` (`pp-ledger-wallet`), its own package — the wallet alone (it signs itself) — for
Linux, macOS and **Windows**. Release builds are reproducible: the same source, compiler and
dependencies give the same bytes; the release publishes the checksums and how to rebuild.

## What it needs from pp-ledger

- **`ACCOUNT_PROOF`**: an account's leaf and Merkle path in the tip block's `stateRoot`.
- **Client verification**, in pp-ledger: replay an authority proof from block 0, check a signed tip,
  check a block against it, check an account proof.
- **The key-backup contract** (WL005).

## Shape of the code

| Piece | Change |
|-------|--------|
| pp-ledger | `ACCOUNT_PROOF`; a verification library (authority, signed tip, block, account proof) |
| `apps_net` | A verified view of the chain: the anchor, several nodes, what each says, what is proven |
| `apps_online` | The wallet's screens |
| `src/apps/wallet` | `pp-ledger-wallet`; its own package; Windows build |

## The repository goes public (WL001)

What it takes, before the first wallet release:

- **The owner's actions:** move the repository to the public organization (`people-post`, beside
  pp-ledger and pp-browser) and make it public; re-add the Apple signing secrets if the move drops
  them.
- **Checked already (wl0):** no key, certificate or credential in the history (only placeholders);
  the MIT license; commit author names and emails become public with the history.
- **After the move:** links to the old address (they redirect), the pp-token-admin README.
