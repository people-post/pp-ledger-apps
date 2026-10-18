# The Admin app

The admin set (the genesis account's keys) mints new tokens, changes the chain's settings, replaces
itself, and certifies the beacon's keys. Each of these is an operation the admins sign, like any
other ([Signing](signing.md)).

## Mint

New tokens go to the reserve only. The chain allows at most a set amount per epoch, the fee
included. At the start that limit is 0, so no minting happens until a config change raises it.

## Config

Each setting the chain lets you change is listed with its value now. Change the ones you mean to,
and *Make the update…*. The change applies from the next epoch; only one change can wait at a time.
The slot length and the epoch length are fixed at the start. When others review a change in a role
app, they see exactly what it changes against the chain as it is now.

## Admin set

Choose the keys of the next admin set (at least 3) and how many must sign (at least 2). The set in
force signs the change, and it applies at once. New admins hand their key cards to you first. The
current admins are ticked.

## Beacon

When the beacon's operator moves to a new signing key, they give you its key card (role "beacon").
Choose it and *Make the certificate…*: once signed and submitted, the beacon signs with the new key.
Do this only together with the beacon's operator.
