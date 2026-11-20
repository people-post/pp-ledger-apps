# Current state — fee app

| Capability | Status | Notes |
|------------|--------|-------|
| Design (fd0) | Done | [DESIGN.md](DESIGN.md): a round's four steps, relays' outside measures, rules kept across rounds, one batch operation per round, evidence on the chain; FD001–FD010 (FD002, FD007 amended as built) |
| pp-ledger: batch operation, producer statistics (fd1) | Done (pp-ledger, unreleased) | Operation kind `batch` (2 to 1,000 transfers, `op::transfers`, `pp-client op batch`); `PRODUCER_STATS` (blocks, fees, misses charged per producer; `pp-client producers`); `op::transfer` carries metadata; L-SMOKE-BATCH |
| Period report (fd2) | Done | `net/FeeOperations` (`readPeriod`, `blockAt`); the relay list and this round's measures (typed or CSV); Period and Relays screens; the draft round in the vault (`fees.json`) |
| Rules and payouts (fd3) | Done | `core/FeeRounds` (rules, `computePayouts`, digest, round files; unit-tested); Rules screen with preview against the last round; Payouts screen |
| Distribute and keep (fd4) | Done | The payouts as one batch from the fee account (a transfer when one account is paid), each carrying `pp-fee-round/1 <number> <end> <digest>`; the round kept and its file saved beside the operation; Rounds screen (save, import — checked by digest); review checks payouts against their round; `ChainLink` submits and follows a batch transfer by transfer. Chain smoke step 5: a round with a relay, signed 3 of 3, paid and marked |
| Package, guide (fd5) | Done | `pp-ledger-fees` (`PP Ledger Fees`, `fees.png`) in the `system` package; [docs/guides/fees.md](../../docs/guides/fees.md) |
