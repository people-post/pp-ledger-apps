# Operator app — phases

Ordering only; status lives in [CURRENT_STATE.md](CURRENT_STATE.md). Design: [DESIGN.md](DESIGN.md).

- [ ] **op0** Design: services, the six screens, the certificate operation file, keys (this folder)
- [ ] **op1** The codec from pp-browser's capabilities repo (OP008: after pp-browser's release 1, when it moves); the certificate operation file and the signing log in `apps_core` (unit-tested against pp-browser's frozen vectors and `relay-operator.mjs`' output)
- [ ] **op2** The Signer opens certificate operation files: Review with the changes against the certificate in force; import of `relay-operator.mjs` key files into the vault
- [ ] **op3** `pp-ledger-operator`, relay kind: Services, New service, Next certificate, Merge and check, Publish; rotate a host key, replace a holder's key; a smoke that makes a 2-of-3 relay certificate and checks it with `relay-operator.mjs verify`
- [ ] **op4** What a host serves, checked against the certificate in force (a relay's `GET /v1/relay-identity`)
- [ ] **op5** Node and ledger relay kinds, once pp-browser and pp-ledger specify their certificates (OP007); renew
- [ ] **op6** Packaged with the system apps; a guide (`docs/guides/operator.md`)
