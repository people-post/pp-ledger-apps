# The genesis ceremony

The chain starts from block 0, signed by its admin set. Signing it in the apps keeps every system
key off the beacon's computer.

## Who does what

- **Every key holder** (admin set, recovery set, reserve, registrar, fee, recycle) makes their key
  in their own vault and hands their key card to the coordinator ([Getting started](getting-started.md)).
- **The beacon's operator** makes the beacon's signing key on the beacon's computer and hands its
  key card (role "beacon") to the coordinator: `pp-client keygen -o <beacon dir>/keys/beacon-signing`
  writes the key where `pp-beacon` looks for it, and its `.pub` is the public half.
- **Genesis miners** hand key cards with role "miner".
- **The coordinator** plans block 0 in PP Ledger Genesis.
- **The admins** sign it.

## Plan

In **Genesis plan**, the coordinator:

1. Names the network and when it starts: a date and time, or `+24` for a day from now. Leave time
   for the signing; the chain waits for the start.
2. For each account and the recovery set, ticks its holders' keys and how many must sign. The admin
   set needs at least 3 keys and 2 signatures. The recovery set must not hold admin keys.
3. Ticks the beacon's key, and the genesis miners with their stakes.
4. Checks the settings. They start from pp-ledger's defaults; the slot length and the epoch length
   cannot change later.
5. *Make the plan…*, and hands a copy to each admin.

## Sign and start

The admins sign their copies in the Signer: it shows every key and account block 0 creates. The
coordinator combines the copies. When the plan is complete, the beacon's operator starts the chain
from it:

```bash
pp-beacon -d <beacon dir> --init --genesis-op signed-plan.json
```

The beacon checks every signature and that the plan certifies its own key, then writes block 0.

## Recovery

If the admin keys are lost or stolen, the recovery set names a new admin set in **Recovery**: tick
the new admins' keys (from their key cards) and how many must sign. The recovery keys sign it, and
it is submitted. The new set takes over after the delay the plan set; the old set can do nothing in
the meantime.
