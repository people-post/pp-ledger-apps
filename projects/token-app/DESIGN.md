# Token app — design

**Status:** design (tk0). Phases: [PHASES.md](PHASES.md) · decisions: [DECISIONS.md](DECISIONS.md) ·
status today: [CURRENT_STATE.md](CURRENT_STATE.md).

A desktop app (Linux, macOS) for the operators of a **token issuer**: an issued account the
registrar created, run by an organization, its keys held by several of its people (multi-signature).
It replaces the planned pp-token-admin repository (TA001): it is the admin apps' sixth app, on their
code, shipped as a package of its own (TA002).

## What a token issuer is, on pp-ledger

- An **issued account** (ids 2²⁰ … 2³⁰−1) is created and funded by the registrar
  ([pp-ledger WIRE_SCHEMA § Account / ledger model](https://github.com/people-post/pp-ledger/blob/develop/docs/contracts/WIRE_SCHEMA.md)).
  The Registrar app makes it for its owners' keys and threshold.
- **Its token is its account id** (TA004). pp-ledger lets an issued account's balance of the token
  with its own id go negative: a transfer of that token out of the account *issues* it, and minus the
  account's balance of it is the supply in circulation. Holders hold it like any token (accounts are
  multi-token).
- **It pays its way in the native token.** Every transfer's fee, and the account's **renewal**: miners
  renew accounts, charging a fee from their native balance. An account that cannot pay is **ended**,
  its balances moved to the recycle account. For an issuer that ends its token's book: the app
  watches the native balance against the renewal fee (TA005).
- **Its keys can be replaced** by the account itself (`T_USER_UPDATE`): the keys in force sign the
  new set.

## What the app does (first version)

| Screen | What |
|--------|------|
| **Overview** | The account: its token (id), supply issued, native balance and what a renewal costs, a warning when the balance runs low; its signers; its latest transactions |
| **Issue** | New tokens to a holder account: a transfer of the issuer's own token (TA004) |
| **Send** | The native token, or the issuer's token, from the account to another |
| **Keys** | Replace the account's keys and threshold (`T_USER_UPDATE`, signed by the keys in force) |
| **Review and submit**, **Keys and people**, **Network** | The admin apps' shared screens: review, sign, merge, submit, follow into a block; the vault and key cards; the node |

Every operation is multi-signature, carried as an operation file like the admin apps' (the Signer
signs them, offline). The vault is the same, with the same strict defaults (TA003).

Not in the first version: the account's attachment and names (pp-ledger reserves their record types,
not wired yet); a list of the token's holders (pp-ledger has no index by token: the overview shows the
issuer's own transactions). Added when needed.

## What it needs from pp-ledger

- **Account updates as operation files** (TA006): a kind for `T_USER_UPDATE` in the operation file —
  its signer account (the updated one), a summary of the new keys and threshold — so reviewers see
  what a key replacement does, like any operation.
- The rest is there: transfers of any token (`operation::transfer`, its summary names the token),
  `CONFIG_GET`, `minimumFeeFor` (a renewal's fee is the account's metadata fee).

## Shape of the code

| Piece | Change |
|-------|--------|
| `apps_net` | Token operations: issue, send, key replacement, built and checked like the admin set's (`AdminOperations`); the account's overview (balances, supply, renewal fee) |
| `apps_online` | The overview, issue, send and keys screens |
| `src/apps/token` | `pp-ledger-token` (`PP Ledger Token`) |
| Packaging | A second set of packages: the Token app and the Signer (TA002) |

## Related: fee distribution (later)

The fee account collects every fee. A later app will manage it so the fees are redistributed to node
operators, with helper tools to analyze operators' contributions (blocks produced, uptime, …). Its
details are not settled; it will be a project of its own, on the same code, when they are.
