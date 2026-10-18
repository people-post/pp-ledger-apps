# Fee app — design

**Status:** built (fd0–fd5); see [CURRENT_STATE.md](CURRENT_STATE.md). Phases: [PHASES.md](PHASES.md) · decisions: [DECISIONS.md](DECISIONS.md) ·
status today: [CURRENT_STATE.md](CURRENT_STATE.md).

A desktop app for the holders of the chain's **fee account**: every fee paid on pp-ledger collects
there, and they hand it on to the people who run the network — its miners and its relays — in
**rounds**, a month or a season apart. It is the apps' seventh app, on their code (FD001), shipped
with the system apps: the fee account's holders are system key holders.

## A round

A round is the four steps of the holders' work, each leaving something the next can stand on:

| Step | What the app does | What it leaves |
|------|-------------------|----------------|
| **1. Gather** | Reads the chain for the round's period — from where the last round ended to the end the holders choose — and takes what the chain cannot know (the relays' work) from the holders | The **period report**: what each miner and relay did, and the fees collected |
| **2. Decide** | Applies the **rules** in force (or edited ones) to the report, and previews who gets what; the holders compare settings until they agree | The **payout table**, and the rules that made it |
| **3. Distribute** | Makes one operation of every payout, signed once by the fee account's holders (Signer), submitted, followed into blocks | The transfers, on the chain |
| **4. Keep** | Writes the **round file** — report, rules, table, operation — whose hash every transfer of the round carries | The evidence; the next round starts from it |

### 1. Gather: the period report

**The period** is a block range (FD002). It starts where the last round ended — the last round kept
in the vault, the holders' rounds shared as round files (step 4) — and the first round at block 1. It
ends where the holders choose: a date (the blocks before it) or the tip. Nodes keep a
year of blocks by default (`checkpoint.minAgeSeconds`); the app reads from a node that holds the
period's (the beacon holds them all).

**From the chain** (anyone can recompute it: FD003), per miner account:

| Measure | From |
|---------|------|
| Blocks produced | Blocks of the period, by `slotLeader` |
| Fees in its blocks | The fees of those blocks' transactions |
| Misses | The misses the chain charged it: slots it led while **due** (no block for `minerDueAfterSlots`) and skipped (FD010). Not every empty slot: an idle chain seals about one heartbeat block an epoch |
| Registered, stake | The account (`Wallet.miner`, native balance), at the period's end |

and the period's totals: blocks, slots, fees collected, the fee account's balance.

**From outside the chain** (FD004): relays sign no blocks, so the chain has no record of their work.
The holders keep a **recipients list** — each relay's name, its address (peer id), and the account
it is paid to — and, per round, enter each relay's measures: what their monitoring says (uptime,
say, as a percentage), typed in or imported from a CSV file. The report keeps them apart from the
chain's, with their source and who entered them; a reviewer sees which numbers they can recompute and
which they take on trust.

### 2. Decide: the rules

**Rules** are kept across rounds and changed when the holders agree (FD005): a round names the rules
it used (their hash). They split the **pot** — what the round hands out:

- **The pot**: the fee account's balance when the period is read, less what the rules keep back (a
  percentage) and a fee for each possible payout.
- **Four shares** (FD009), adding up to 100%: miners by blocks produced, miners by the fees in their
  blocks, relays by uptime, and everyone paid this round equally — e.g. 60%, 0%, 30%, 10%.
- **Adjustments**: a miner's blocks less `missPenalty` a miss (floored at 0); miners no longer
  registered left out (a setting); nothing paid under the smallest payout (it stays for a later
  round). Rounding is down; what it leaves stays in the fee account.

The app shows the payout table as the rules make it, beside the previous round's; the holders change
settings, compare, and keep the settings they agree on (as new rules, or this round only).

### 3. Distribute: one operation

A round pays tens of recipients. One operation file per transfer would ask every holder to review and
sign each: instead the round is **one batch operation** (FD006) — every transfer of the round, from
the fee account, reviewed as a table (recipient, amount, total, fees) and signed once by each holder
in the Signer, as the genesis plan's records are. Submitting sends each transfer; the app follows
them into blocks and reports any refused (e.g. a recipient account ended) for a follow-up.

### 4. Keep: the round file

The round file holds the period, the report (with the outside inputs and their source), the rules and
the payout table, and their **digest**; it is saved beside the operation and kept in the vault.
**Each transfer of the round carries `pp-fee-round/1 <number> <end block> <digest>`** (FD007): a
holder reviewing the payouts sees them checked, account by account, against the round file their
digest names; anyone holding a round file checks it against what the chain paid. Holders share round
files (Rounds: save, import — an imported file is refused unless its digest matches), so each knows
where the next round starts.

## Screens

| Screen | What |
|--------|------|
| **Rounds** | The rounds so far (kept in the vault; others' as round files), the fee account's balance; start the next |
| **Period** | Its end (date or tip); reading the chain, with progress; the chain's measures per miner |
| **Relays** | The recipients list; this round's measures per relay (typed or imported) |
| **Rules** | The rules in force; edit, preview, compare with the last round's table |
| **Payouts** | The table; make the batch operation |
| **Review and submit**, **Keys and people**, **Network** | The apps' shared screens |

## What it needs from pp-ledger (1.2)

- **A batch operation** (FD006): 2 to 1,000 transfers from one account in one operation file, each
  record signed by the account's keys; summarized as a table; submitted record by record.
- **Producer statistics** (`PRODUCER_STATS`): a block range's blocks, fees and misses charged per
  producer, 10,000 blocks a reply — nodes record the misses they charge as they apply blocks.

## Shape of the code

| Piece | Change |
|-------|--------|
| `apps_net` | `FeeOperations`: the period report from the chain, the block at a date, the payouts' operation; `ChainLink` submits and follows a batch transfer by transfer |
| `apps_core` | `FeeRounds`: rules, the payout computation (pure: report and rules in, table out — unit-tested), round files and their digest, the vault's records (relays, rules, draft, rounds) |
| `apps_online` | The screens above |
| `src/apps/fees` | `pp-ledger-fees` (`PP Ledger Fees`), in the `system` package |

Not in the first version: periodic rounds (the holders start each round; once the rules settle,
the app may propose the next round on a schedule — FD008); measuring relays itself; finding the last
round on the chain alone (a node lists an account's transactions 32 blocks a page: too slow over a
season — the round files carry it, FD002).
