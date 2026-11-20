# The Fees app

For the holders of the chain's **fee account**: every fee paid on the chain collects there, and you
hand it on to the people who run the network — its miners and its relays — in **rounds**, a month or
a season apart. Each round is one operation from the fee account, signed by enough of you.

Install the `pp-ledger-system` set ([Getting started](getting-started.md)); your keys are *Fee* keys.

## A round, step by step

1. **Rounds → Start the next round.** The Rounds screen lists the rounds so far and the fee account's
   balance.
2. **Period.** The round starts where the last one ended (filled in). Choose where it ends — a date
   (`2026-10-31`, or `2026-10-31 18:00`, in UTC), or leave it empty for now — and *Read the chain*.
   You see the blocks, the fees in them, and each miner: blocks produced, fees in its blocks,
   **misses** (slots it was chosen for while it owed a block, and skipped — an idle chain's empty
   slots are not misses), and whether it is still a registered miner.
3. **Relays.** Relays sign nothing on the chain, so the round takes their work from you: list each
   relay, its peer id and the account it is paid to (kept for later rounds), then enter this round's
   **uptime** for each — typed, or *Import uptime (CSV)…* from a file of `relay,uptime` lines (the
   relay's name or peer id; `99.5` or `99.5%`). Say who entered them and where they come from: the
   round file keeps both, and reviewers see these numbers are taken on trust. *Keep them.*
4. **Rules.** What is kept back of the balance, and how the rest is shared out, in four shares adding
   up to 100%: miners by blocks produced, miners by the fees in their blocks, relays by uptime, and
   everyone paid this round equally. Also: how many blocks a miss costs, the smallest payout (smaller
   ones wait for a later round), and whether to leave out miners no longer registered. *Preview the
   payouts* (the last round's beside them), adjust, and *Keep as the rules in force* — later rounds
   use them until you change them.
5. **Payouts.** The table, the totals and the round's **digest**. *Make the operation…*: save it, and
   its round file is saved beside it (`payouts-3.round.json` beside `payouts-3.json`). Then sign,
   hand both files to the other holders, collect their signatures and submit, as for every operation
   ([Signing](signing.md)). Each payout carries the round's number, end and digest.

## Checking a round

When you review payouts in the Fees app, it checks them against the round file their digest names:
*the same payouts, account by account* — or **different payouts: do not sign**. If you were not the one
who made the round, import its round file first (**Rounds → Import a round file…**): a file whose
contents do not match its digest is refused.

Keep every round file, and publish them as you agree: with one, anyone can check what the chain paid
against why. The next round starts where the last round you hold ended — import the others' rounds so
everyone starts at the same block.
