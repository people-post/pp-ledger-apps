# Fee app — phases

Ordering only; status lives in [CURRENT_STATE.md](CURRENT_STATE.md). Design: [DESIGN.md](DESIGN.md).

- [ ] **fd0** Design: rounds, the report, rules, the batch operation, evidence (this folder)
- [ ] **fd1** pp-ledger: the batch operation (FD006); a slot's elected leader; reading a block range
- [ ] **fd2** The period report: from the chain (`apps_net`), the recipients list and outside inputs; Period and Relays screens
- [ ] **fd3** Rules and payouts: the rules file, the payout computation (`apps_core`, unit-tested), Rules and Payouts screens
- [ ] **fd4** Distribute and keep: the batch operation from the table, round files, the Rounds screen; the chain smoke runs a round
- [ ] **fd5** `pp-ledger-fees` in the `system` package; its guide
