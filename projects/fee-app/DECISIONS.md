# Fee app — decisions

Rationale for [DESIGN.md](DESIGN.md). Outcomes move to `docs/` when they ship; mark the entry
**superseded by** that doc then.

## FD001 — The fee app is built here

**Date:** 2026-10-18
**Decision:** The fee account's app is a seventh app in this repository, shipped in the `system`
package with the other system key holders' apps.
**Rationale:** The fee account is a multi-signature system account like the reserve: its holders need
the vault, key cards, operation files signed apart, the Signer, submitting and following — all here.
**Alternatives:** A repository of its own (a copy of all that).

## FD002 — A round is a block range, chained to the last

**Date:** 2026-10-18
**Decision:** A round covers the blocks after the last round's end block up to an end the holders
choose (a date, or the tip). The start is read from the chain (FD007), not typed.
**Rationale:** Rounds that neither overlap nor leave gaps, whatever the calendar: every block is
counted once. Holders think in months or seasons; the app turns a date into the last block before it.
**Alternatives:** Calendar periods (blocks at the edges counted by timestamps; gaps when a round is
late); epoch ranges (not how the holders think).

## FD003 — The chain's measures are recomputable

**Date:** 2026-10-18
**Decision:** What the report takes from the chain (blocks produced, fees in them, slots missed,
registration, stake) is computed from the period's blocks and accounts only, so any holder's app
computes the same report.
**Rationale:** Holders sign a payout; they should be able to check its inputs, not trust whoever made
the table.
**Alternatives:** Reports made by one operator's tooling and shared as files.

## FD004 — Outside measures are entered, and kept apart

**Date:** 2026-10-18
**Decision:** Relays' work (not on the chain) is entered by the holders per round — typed, or
imported from their monitoring as CSV — against a recipients list kept in the vault (a relay's name,
peer id, and payout account). The report keeps these inputs apart, with their source and who entered
them.
**Rationale:** Relays are paid, but sign nothing on the chain. Reviewers must see which numbers they
can recompute and which they take on trust.
**Alternatives:** Relays measure themselves on the chain (a protocol change); relays not paid.

## FD005 — Rules are a file kept across rounds

**Date:** 2026-10-18
**Decision:** The rules (what is kept back, the shares, the measure each is split by, adjustments,
minimums) are a file; a round names the rules it used by their hash. Changing them makes new rules;
a round may also apply one-off settings.
**Rationale:** Rules change over time but hold for a few rounds: kept, they make each round "check
and sign"; named, the round file says which applied.
**Alternatives:** Settings chosen each round (every round a negotiation); rules fixed in code.

## FD006 — A round is one batch operation

**Date:** 2026-10-18
**Decision:** pp-ledger's operation file gains a batch of transfers from one account: one file,
reviewed as a table, each record signed by each holder in one go; submitted record by record.
**Rationale:** A round pays tens of recipients; an operation each would ask every holder to sign tens
of files.
**Alternatives:** A transfer per file (tens of signatures per holder per round); a new multi-recipient
transaction type (a protocol change, where a batch of existing transfers is not).

## FD007 — Evidence: a round file, its hash on the chain

**Date:** 2026-10-18
**Decision:** The round file holds the period, report, outside inputs, rules, table and operation.
Every transfer of the round carries the file's hash and the round's end block in its metadata.
**Rationale:** The chain alone then says where the last round ended (FD002) and what it paid; the file
says why, and is checked against the hash. The holders keep and publish the files as they choose.
**Alternatives:** Files only (the next round's start depends on whoever kept them); everything on the
chain (a season's report is too large for transaction metadata).

## FD008 — Rounds are started by the holders

**Date:** 2026-10-18
**Decision:** The first version has no schedule: the holders start a round when they choose.
**Rationale:** Rounds are roughly monthly or seasonal while the rules are worked out; a schedule can
come once they settle (the app proposing the next round, prefilled).
**Alternatives:** A fixed schedule from the start.
