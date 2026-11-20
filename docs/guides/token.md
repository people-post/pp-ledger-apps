# The Token app

For the people who run a **token issuer**: an account the chain's registrar created for your
organization (its number between 1,048,576 and 1,073,741,823). Its token is that number. Several of
you hold its keys, and a set number of you must sign everything it does.

Install **PP Ledger Token** and **PP Ledger Signer** (the `pp-ledger-token` packages; see
[Getting started](getting-started.md) for installing, the vault and key cards). Under **Keys**, make
your key as an *Account owner* key, back up the vault, and hand your key card to whoever asks the
registrar for the account. The registrar creates it for your keys and tells you its number.

## Overview

Type the account's number and *Check and save* (once: it is kept in your vault). The overview shows:

- **Its symbol:** the account's domain, which the registrar gives (4 to 7 letters, never changed):
  what holders see next to their amounts and can send it by.
- **Issued and out:** how much of the token holders have, all together.
- **Native balance** and **what a renewal costs.** The chain renews every account now and then and
  charges the fee from its native balance. **An account that cannot pay is ended, and its token's
  book with it.** The overview warns when the balance covers only a few renewals: ask the registrar
  (or anyone holding native tokens) to send it more.
- **Its signers**, and its latest transactions.

## Issue

New tokens to a holder: the holder's account number, the amount, the fee (the smallest the chain
takes is filled in; fees are paid in the native token). *Make the operation…*, then sign, collect
the other signers' signatures and submit, as for every operation ([Signing](signing.md)).

## Send

Native tokens, or another token the account holds, to another account. Leave *Token* empty for the
native token. Its own token is issued (above), not sent.

## Signers

Replace the account's keys — someone joins or leaves, a key is lost. The keys in force are ticked;
untick the leaving ones, tick or add (*Add a key card…*) the joining ones, and choose how many must
sign. *Work it out* fills in the fee. The keys in force sign the change; once it is in a block, the
new keys sign everything from then on.

A lost key cannot sign: replace it while enough of the others can still sign.

## Its recovery

If too many of the keys are lost or stolen, the account is lost — unless it has a **recovery set**:
keys above the keys in force, kept apart (in another office, with a trusted party), which can give it
new keys. Under *Its recovery* on Signers: add each recovery key's card, choose how many must sign and
the days the account waits for new keys, then *Set the recovery…* — an operation the keys in force
sign. After that only the recovery keys change it: *Change the recovery set…* and *Remove the
recovery…* make operations for them to sign.

To recover, the recovery keys' holders use the wallet (Recovery → *As an account's recovery set*):
they name the issuer's new keys. **The account is frozen at once** — no issuing, no sending — and the
new keys take over at the first epoch start after the delay. Overview warns of it; if the issuer did
not ask for it, its recovery keys' holders withdraw it (*Withdraw the recovery…* on Signers, or the
wallet).
