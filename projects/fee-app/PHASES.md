# Fee app — phases

Ordering only; status lives in [CURRENT_STATE.md](CURRENT_STATE.md). Design: [DESIGN.md](DESIGN.md).

- [x] **fd0** Design: rounds, the report, rules, the batch operation, evidence (this folder)
- [x] **fd1** pp-ledger: the batch operation (FD006); producer statistics over a block range (blocks, fees, misses charged)
- [x] **fd2** The period report: from the chain (`apps_net`), the recipients list and outside inputs; Period and Relays screens
- [x] **fd3** Rules and payouts: the rules file, the payout computation (`apps_core`, unit-tested), Rules and Payouts screens
- [x] **fd4** Distribute and keep: the batch operation from the table, round files, the Rounds screen; the chain smoke runs a round
- [x] **fd5** `pp-ledger-fees` in the `system` package; its guide
