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

If too many of the keys are lost to sign anything, the account is lost — unless it has a **recovery
set**: keys kept apart (in another office, with a trusted party) that can name its new keys. Under
*Its recovery* on Signers: add each recovery key's card, choose how many must sign and the days you
have to cancel in, then *Set the recovery…* — an operation the keys in force sign, like a key
replacement. *Remove the recovery…* takes it away.

To recover, the recovery keys' holders use the wallet (Recovery → *Recover an account*): they name the
issuer's new keys, and those take over when the days have passed. Until then the account works as
before, and **any change signed by the keys in force cancels it**: if Overview warns of a recovery you
did not ask for, *Cancel the recovery…* on Signers.
