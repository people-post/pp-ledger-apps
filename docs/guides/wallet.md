# The Wallet

**PP Ledger Wallet** is your last resort: a wallet that depends on nothing you may have stopped
trusting — no plugins, no web pages, no automatic updates — and that believes no node. Use it to see
what you hold, to send it, and above all to **move your funds away from keys that may have leaked**
(through a browser plugin, a lost computer, a doubtful app).

Install it on its own: the `pp-ledger-wallet` package or AppImage (Linux), its disk image (macOS).
Check the download against the release's `SHA256SUMS` ([how to rebuild it](../REPRODUCIBLE.md)).

## The first time

1. **Create your vault** — your keys, encrypted with a passphrase only you know
   ([Getting started](getting-started.md#your-vault)).
2. **Pin your network** (Network): import the **network card** its operators publish — its name, the
   hash of its first block, a few nodes — or type them. **Check that hash against a second source** you
   trust (the operators' site, someone you know): the wallet checks everything it shows back to it, so a
   wrong hash means a wrong chain.
3. **Check the nodes**: each one's latest block, signed by the network's beacon and checked back to the
   first block. A node far behind the others is flagged.
4. **Bring your keys** (Keys): *Import a key backup…* — the file your other wallet exports (a
   `.ppkey`, under its passphrase) — or a key file from `pp-client keygen`. Then **back the vault up**.
5. **Add your accounts** (Accounts) by number.

## What "proven" means

Next to each account: *proven at block N*. The node you asked gave the account with a proof — the
block's signed hash back to the first block, and the account's place in that block's state — and the
wallet checked it. A node can refuse to answer, or answer late; it cannot make the wallet show a balance
the chain does not hold. An account that does not exist is *proven absent*. What the wallet cannot prove
(the renewal fee, from the node's settings) it marks as *the node's word*.

## Send and receive

- **Receive**: give the payer your account's number.
- **Send** (Send… on an account): the account it goes to and the amount (and the token, if not the
  native one). The wallet proves both accounts, then shows what the transfer does. If this wallet holds
  enough of the account's keys: *Sign and send*; it follows the transfer into a block and proves the
  balance again. If others must sign too: *Save as an operation file…* and hand it to them
  ([Signing](signing.md)).

## Evacuate

If a key of an account may have leaked, act at once (Evacuate… on the account):

- **Replace its keys with a new one**: a new key, made in this wallet, becomes the account's only key;
  the old keys sign the change and nothing after it.
- **Or move everything** to another account of yours, whose keys never were where the leak was: every
  token it holds, then the native token less the fees.

Either way the wallet proves the result when it is in a block. **Back the vault up afterwards**: the new
key exists nowhere else.

## Keys in and out

- **Save key backup…** (Keys) writes one key to a `.ppkey` file under the backup passphrase: any wallet
  that reads key backups ([the format](https://github.com/people-post/pp-ledger/blob/develop/docs/contracts/KEY_BACKUP.md))
  takes it.
- **Import a key backup…** brings one in.
- A key backup is as strong as its passphrase: keep the file and the passphrase apart.

## Offline

On a computer that never connects, the wallet signs without a network: open an operation file
(*Review and sign*), sign, and carry the signed copy back to the connected wallet to send.
