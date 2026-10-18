# Token app — decisions

Rationale for [DESIGN.md](DESIGN.md). Outcomes move to `docs/` when they ship; mark the entry
**superseded by** that doc then.

## TA001 — The token app is built here; pp-token-admin is retired

**Date:** 2026-10-18
**Decision:** The token issuers' app is a sixth app in this repository, on the admin apps' libraries.
The pp-token-admin repository (empty) is archived, its README pointing here.
**Rationale:** Issuers are multi-signature accounts operated by organizations: they need what the admin
apps have — the vault, key cards, operation files signed apart and merged, the offline Signer, building
against the chain, submitting, packaging and signing. Here it is one more app; apart, it is a copy of
all that, or a shared library repository first.
**Alternatives:** A separate repository (the earlier plan, for a wider audience than the issuers are).

## TA002 — Its own package

**Date:** 2026-10-18
**Decision:** Releases carry two sets of packages: the system apps (Genesis, Admin, Reserve,
Registrar, Signer) and the token app with the Signer.
**Rationale:** Issuers' operators are not the chain's key holders: they install what they use, not the
minting and genesis apps.
**Alternatives:** One package of six apps (simpler releases; every issuer sees the system apps).

## TA003 — The same strict vault

**Date:** 2026-10-18
**Decision:** The token app uses the vault as the admin apps do: Argon2id at libsodium's sensitive
limits, a passphrase always, the 10-minute lock.
**Rationale:** Its users are operators of organizations holding an issuer's keys — value on the scale
of the system accounts', not casual users.
**Alternatives:** Lighter unlocking for the token app (faster; weaker against a stolen vault).

## TA004 — A token is its issuer's account

**Date:** 2026-10-18
**Decision:** The app treats an issuer's token as pp-ledger does: its id is the issuer account's id;
issuing is a transfer of it out of the issuer; the supply is minus the issuer's balance of it.
**Rationale:** That is the chain's rule (an issued account may go negative in its own token only); no
second notion of a token to keep in step.
**Alternatives:** Token metadata (names, decimals) on the account's attachment — later, when the
attachment records are wired.

## TA005 — The app watches the account's renewal

**Date:** 2026-10-18
**Decision:** The overview shows the native balance against the renewal fee and warns when it runs low.
**Rationale:** An account that cannot pay its renewal is ended and its balances go to the recycle
account — for an issuer, the end of its token's book. Operators must see it coming.
**Alternatives:** Leave it to the operators (the chain does not warn).

## TA006 — Key replacement is an operation file like the others

**Date:** 2026-10-18
**Decision:** pp-ledger's operation file gains a kind for account updates (`T_USER_UPDATE`): signed by
the updated account, summarized as its new keys and threshold.
**Rationale:** Reviewers trust what the app computes from the bytes (AD007); a key replacement shown as
an unknown record would ask them to sign blind.
**Alternatives:** Wrap it as an unknown record (no summary, no signer check).
