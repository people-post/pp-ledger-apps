# Admin apps — design

**Status:** design (ad0). Phases: [PHASES.md](PHASES.md) · decisions: [DECISIONS.md](DECISIONS.md) ·
status today: [CURRENT_STATE.md](CURRENT_STATE.md).

Desktop apps (Linux, macOS) for the people who hold a pp-ledger chain's system keys. They are not
engineers: every step is a form or a file, never a command line or a JSON edit.

## Who uses them, for what

The chain's authority sits in a few multi-signature accounts and key sets
([pp-ledger WIRE_SCHEMA § Account / ledger model](https://github.com/people-post/pp-ledger/blob/develop/docs/contracts/WIRE_SCHEMA.md),
[BEACON_AUTHORITY.md](https://github.com/people-post/pp-ledger/blob/develop/docs/contracts/BEACON_AUTHORITY.md)). Each group of holders
gets an app for what only they may start:

| App | For | Starts (creates operations) | Records on chain |
|-----|-----|-----------------------------|------------------|
| **Genesis** | The people who kick the chain off and hold the restore (recovery) keys | Key cards; the genesis plan, signed offline; replacing the admin set after its keys are lost or stolen | `T_GENESIS` (and the genesis block's system accounts, miners); `T_RECOVERY` |
| **Admin** | The holders of the admin (genesis account) keys | Minting to the reserve; chain config updates; certifying a new beacon key; rotating the admin keys | `T_DEFAULT` genesis → reserve; `T_CONFIG`; `T_BEACON_CERT` |
| **Reserve** | The holders of the reserve account | Transfers out of the treasury (to the registrar, to users, …) | `T_DEFAULT` from reserve |
| **Registrar** | The holders of the account-creation (registrar) account | Creating operator-issued accounts (ids 2^20 … 2^30), funding them | `T_NEW_USER` from the registrar |
| **Signer** | Every key holder | Nothing: opens an operation, shows what it does, signs it, merges signed copies | — |

The four role apps **create** operations and **submit** them once signed; the Signer only reviews
and signs, **offline**. A person holding several roles installs several apps; all of them use the
same vault on that machine (§ Vault).

## How an operation travels

```
 role app (online or not)        each signer (offline)           role app (online)
 ─────────────────────────       ──────────────────────          ──────────────────
 fill a form ──► operation file ──► Signer: review, sign ──┐
                     │                                      ├──► merge copies ──► submit ──► included ✓
                     └──────────────► Signer: review, sign ─┘
```

1. **Create.** A role app turns a form into an operation file: the transaction, built with pp-ledger's
   own code, plus who must sign and by when. It needs no network, except to read what the form
   shows of the chain (a config's current values, a balance).
2. **Hand out.** The file holds no secret: USB stick, email or chat all do. Each signer gets a copy.
3. **Sign, in parallel.** In the Signer, a holder opens the file, reads what it does (recomputed from
   the transaction itself), checks who has signed, and signs with a key from their vault. Nobody waits
   for anybody: each signs their own copy.
4. **Merge.** Any app combines signed copies of the same operation into one (signatures of the same
   transaction bytes, nothing else may differ).
5. **Submit.** Once the quorum is met, the role app sends it to a relay or the beacon (same bytes on
   retry: pp-ledger's `Client::submitTransaction`) and follows it into a block.

## The operation file

The normative format lives in pp-ledger as a contract (`docs/contracts/OPERATION_FILE.md`), with a
library that reads, builds, verifies and merges it; `pp-client` speaks it too, so engineers and
operators exchange the same files. In outline, JSON:

| Field | Holds |
|-------|-------|
| `format` | `pp-ledger-operation`, version |
| `network` | network id and genesis hash: it cannot be submitted to another chain |
| `kind` | `mint`, `config-update`, `beacon-cert`, `recovery`, `transfer`, `new-account`, `genesis`, … |
| `transaction` | the record's exact bytes (base64): what is signed |
| `signers` | the account (or key set) that must sign: its public keys and threshold |
| `signatures` | collected so far: key fingerprint, signature |
| `notes` | free text from the creator (shown as theirs, never as the operation's meaning) |

Rules the apps hold to:

- **The meaning comes from the bytes.** What an app shows ("Mint 1,000,000 to Reserve, valid until
  Tue 14:00") is recomputed from `transaction`; nothing else in the file is trusted. Any change to
  the bytes breaks every signature, so tampering shows.
- **Every signature is checked** against `signers` on opening; a copy with a bad one is refused.
- **A deadline.** A transaction is valid for a window the chain caps (`maxValidationTimespanSeconds`,
  default one day). Apps create the widest window allowed and show the deadline everywhere; an
  expired operation is made again. A network whose signers need days asks the admins to raise the
  cap (a config update).
- **Submitting twice is harmless**: the pool keeps one copy, the chain one transaction per payer and
  idempotent id.

## The genesis ceremony (offline-signed genesis)

Today `pp-beacon --init` signs the genesis block itself, with two genesis private keys on the beacon
host (`PP_GENESIS_KEY_FILES`). The ceremony keeps every system key offline instead; pp-ledger gains
an init from a genesis signed elsewhere:

1. **Key cards.** Each participant creates their keys in their vault (Genesis app, or the role app
   they will use) and exports a *key card*: name, role, public key, fingerprint.
2. **The plan.** A coordinator collects the cards and builds the genesis plan in the Genesis app:
   the network id, the chain config (defaults explained in plain words), the system accounts
   (genesis/admin set and its threshold, reserve, registrar, fee, recycle), the recovery set, the
   genesis miners, the first beacon key. The plan is an operation file of kind `genesis`.
3. **Signing.** The admin set signs it in the Signer, like any operation.
4. **Start.** The beacon operator runs `pp-beacon --init` with the signed file: the beacon checks it
   and writes block 0. No system private key ever touches the beacon host.

The key cards double as the **address book**: apps show names, not account ids or key hex.

## Vault

From pp-browser's at-rest encryption ([AT_REST_ENCRYPTION.md](https://github.com/people-post/pp-browser/blob/main/docs/contracts/AT_REST_ENCRYPTION.md)),
for keys of higher value:

- **One vault per person per machine**, shared by the apps (`~/.pp-ledger-admin/` on Linux,
  `~/Library/Application Support/pp-ledger-admin/` on macOS). Each key carries a label and a role;
  an app lists the keys of its role, the Signer any.
- **A passphrase, always** — no default PIN. Argon2id derives the key that wraps a random 32-byte
  data key; XChaCha20-Poly1305 encrypts each key file under it; writes are atomic (temp, fsync,
  rename). Argon2id at libsodium's *sensitive* limits: unlock is rare and the keys are worth it.
- **Private keys are never shown.** They leave the vault only in an encrypted backup.
- **Backup**: an export of the vault re-encrypted under its own passphrase, to a USB stick; restore
  on a new machine. Losing an admin key is far worse than losing a chat profile — the app asks for a
  backup when a key is created and reminds until one exists.

## Shape of the code

One repo, one CMake build, five executables:

| Piece | What | Depends on |
|-------|------|-----------|
| `admin_core` | Vault, key cards / address book, operation workflow (open, verify, sign, merge), app settings | pp-ledger operation library, pp-cpp-crypto |
| `admin_net` | Reading chain state, submitting, following inclusion | pp-ledger `pp::client` |
| `admin_ui` | Shared screens: vault unlock, key list, operation review (the same view in every app), file open/save | pp-cpp-ui (SDL3 + OpenGL, RML documents) |
| `pp-admin-genesis`, `pp-admin-admin`, `pp-admin-reserve`, `pp-admin-registrar` | Their forms | core, net, ui |
| `pp-admin-signer` | Review, sign, merge | core, ui — **not** `admin_net`: the Signer has no network code at all |

The stack follows pp-browser (C++, pp-cpp-ui, pp-cpp-crypto); pp-ledger comes in as a library
(`PP_LEDGER_BUILD_APPS=OFF`), so every transaction is built and checked by the chain's own code.
Packages: AppImage / `.deb` on Linux, a signed and notarized `.dmg` on macOS.

## What it does not do

- Hold keys of ordinary users or miners (wallets are pp-browser's).
- Talk to a node from the Signer.
- Edit an operation after it is created: a change is a new operation (old signatures would not hold).

## Open questions

- **pp-token-admin**: token issuers are accounts the registrar creates; does token administration
  join these apps or stay apart?
- **Amounts**: the native token's display unit and decimals (operators read "1,000,000", not raw
  integers).
- **Hardware keys**: ML-DSA-65 on a hardware token is not common yet; vault-only for now.
- **Who submits**: any holder with network, or a designated operator per role?
