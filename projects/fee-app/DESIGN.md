# Fee app — design

**Status:** design (fd0). Phases: [PHASES.md](PHASES.md) · decisions: [DECISIONS.md](DECISIONS.md) ·
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

**The period** is a block range (FD002). It starts after the last round's end block, found on the
chain (step 4: the fee account's latest round transfers say where it ended); the first round starts at
block 1. It ends where the holders choose: a date (the last block before it) or the tip. Nodes keep a
year of blocks by default (`checkpoint.minAgeSeconds`); the app reads from a node that holds the
period's (the beacon holds them all).

**From the chain** (anyone can recompute it: FD003), per miner account:

| Measure | From |
|---------|------|
| Blocks produced | Blocks of the period, by `slotLeader` |
| Fees in its blocks | The fees of those blocks' transactions |
| Slots missed | Slots of the period with no block, by their elected leader (pp-ledger: exposed from the election inputs blocks commit — `epochSeed`, the stake snapshot) |
| Registered, stake | The account (`Wallet.miner`, native balance), at the period's end |

and the period's totals: blocks, slots, fees collected, the fee account's balance.

**From outside the chain** (FD004): relays sign no blocks, so the chain has no record of their work.
The holders keep a **recipients list** — each relay's name, its address (peer id), and the account
it is paid to — and, per round, enter each relay's measures: what their monitoring says (uptime,
say, as a percentage), typed in or imported from a CSV file. The report keeps them apart from the
chain's, with their source and who entered them; a reviewer sees which numbers they can recompute and
which they take on trust.

### 2. Decide: the rules

**Rules** are a file of their own, kept across rounds and changed when the holders agree (FD005): a
round names the rules it used (their hash). They split the **pot** — what the round hands out:

- **The pot**: the fee account's balance at the period's end, less what the rules keep back (a share
  or an amount).
- **Shares**: the pot in parts, each split among the recipients it names by one measure — e.g. 60% to
  miners by blocks produced, 30% to relays by uptime, 10% to every recipient active the whole period,
  equally.
- **Adjustments**: e.g. a miner's blocks less its missed slots (floored at 0); recipients deregistered
  during the period, or below a minimum, left out; nothing paid under a minimum payout (it stays for
  the next round).

The app shows the payout table as the rules make it, beside the previous round's; the holders change
settings, compare, and keep the settings they agree on (as new rules, or this round only).

### 3. Distribute: one operation

A round pays tens of recipients. One operation file per transfer would ask every holder to review and
sign each: instead the round is **one batch operation** (FD006) — every transfer of the round, from
the fee account, reviewed as a table (recipient, amount, total, fees) and signed once by each holder
in the Signer, as the genesis plan's records are. Submitting sends each transfer; the app follows
them into blocks and reports any refused (e.g. a recipient account ended) for a follow-up.

### 4. Keep: the round file

The round file holds the period, the report (with the outside inputs and their source), the rules, the
payout table and the operation. **Each transfer of the round carries the file's hash and the round's
end block in its metadata** (FD007): the chain alone says when the last round ended and what it paid;
the file, kept by the holders (and published as they choose), shows why — anyone holding it can check
it matches what the chain paid.

## Screens

| Screen | What |
|--------|------|
| **Rounds** | The rounds so far (from the chain and the round files kept), the fee account's balance; start the next |
| **Period** | Its end (date or tip); reading the chain, with progress; the chain's measures per miner |
| **Relays** | The recipients list; this round's measures per relay (typed or imported) |
| **Rules** | The rules in force; edit, preview, compare with the last round's table |
| **Payouts** | The table; make the batch operation |
| **Review and submit**, **Keys and people**, **Network** | The apps' shared screens |

## What it needs from pp-ledger

- **A batch operation** (FD006): several transfers from one account in one operation file, each record
  signed by the account's keys; summarized as a table; submitted record by record. (The genesis plan
  already carries several records, signed alike.)
- **A slot's elected leader** for slots with no block — computable from what blocks commit; exposed so
  clients need not reimplement the election.
- **Reading a block range** efficiently enough for a season of blocks (fetching blocks one by one is
  slow at that scale): a range read, or the period's per-leader counts computed by the node.

## Shape of the code

| Piece | Change |
|-------|--------|
| `apps_net` | The period report from the chain; the batch operation; following a round's transfers |
| `apps_core` | Rules, the payout computation (pure: report and rules in, table out — unit-tested), round files, the recipients list (in the vault) |
| `apps_online` | The screens above |
| `src/apps/fees` | `pp-ledger-fees` (`PP Ledger Fees`), in the `system` package |

Not in the first version: periodic rounds (the holders start each round; once the rules settle,
the app may propose the next round on a schedule — FD008); measuring relays itself.
