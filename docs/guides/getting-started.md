# Getting started

These apps are for the people who hold a pp-ledger chain's system keys, and for those who run a
token issuer. Each of you uses the app for your role, and the Signer to review and sign what others
make.

| App | For |
|-----|-----|
| **PP Ledger Signer** | Everyone holding a key: review an operation, sign it, combine signed copies. Works offline |
| **PP Ledger Reserve** | The reserve's holders: transfers out of the treasury |
| **PP Ledger Admin** | The admin set: minting, config changes, the admin set itself, beacon keys |
| **PP Ledger Registrar** | The registrar's holders: creating accounts for their owners |
| **PP Ledger Genesis** | The people who start the chain and hold its recovery keys |
| **PP Ledger Fees** | The fee account's holders: handing the collected fees on to miners and relays ([Fees](fees.md)) |
| **PP Ledger Token** | A token issuer's operators: issuing its token, its balance, its signers ([Token](token.md)) |
| **PP Ledger Wallet** | Everyone: your own accounts, proven, and moving them away from leaked keys — the last resort ([Wallet](wallet.md)) |

## Install

Each release has two sets: **pp-ledger-system** (the Signer, Reserve, Admin, Registrar, Fees and
Genesis apps) for the system key holders, and **pp-ledger-token** (the Signer and the Token app) for token
issuers.

- **macOS (Apple silicon, macOS 13 or later):** open your set's disk image and drag the apps you need
  to Applications.
- **Linux (Ubuntu 24.04 or later, and similar):** install your set's `.deb` with the Signer's, in one
  command: `sudo apt install ./pp-ledger-system_…_amd64.deb ./pp-ledger-signer_…_amd64.deb` (token
  issuers: `./pp-ledger-token_…_amd64.deb ./pp-ledger-signer_…_amd64.deb`). Or download the AppImage
  of one app, make it executable (`chmod +x`), and run it.

## Your vault

The first time an app opens, it asks you to create your **vault**: where your keys are kept on this
computer, encrypted with a passphrase only you know. All the apps on this computer share it.

- **Choose a passphrase of a few words you will remember.** Nobody can recover a forgotten
  passphrase, and the keys in the vault cannot be used without it.
- The app locks itself after 10 minutes without use. Unlock it with your passphrase.
- Your keys never leave the vault except encrypted, in a backup.

## Your keys

Under **Keys and people**:

1. **Make a key** for your role (name it: whose key, what for; a token issuer's: *Account owner*). Or **import** one made with
   `pp-client keygen` (its `.key` and `.pub` files, side by side).
2. **Back up the vault at once:** *Write a backup…* to a USB stick, under a backup passphrase of its
   own. Keep the stick and that passphrase apart, away from this computer. A lost key cannot be made
   again. To restore on a new computer, create a vault there and *Restore from a backup…*.
3. **Save your key card** and hand it to whoever coordinates your role (the genesis coordinator, the
   person who makes your role's operations). A key card holds your public key only, never a secret.
4. **Add the cards of the people you work with**, so the apps show their names instead of long
   numbers. Check each card's fingerprint (four groups of four characters) with its owner, by phone
   or in person.

## The node (role apps)

The Reserve, Admin, Registrar, Genesis, Fees and Token apps talk to one node of the chain: a relay or the beacon.
Its operator gives you its address (it starts with `/ip4/` or `/dns4/` and ends with `/p2p/` and a
long id). Paste it under **Network** and press *Check and save*. The Signer never connects to
anything.

Next: [Signing](signing.md) · [Reserve](reserve.md) · [Admin](admin.md) · [Registrar](registrar.md) ·
[The genesis ceremony](genesis-ceremony.md) · [Fees](fees.md) · [Token](token.md) · [Wallet](wallet.md)
