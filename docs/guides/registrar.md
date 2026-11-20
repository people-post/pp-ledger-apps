# The Registrar app

The registrar creates the accounts the chain issues (numbers from 1,048,576 to 1,073,741,823) and
funds them.

1. Under **New account**, type the account's number, or *Pick a free one*.
2. Add its owner's key: their key card, or the `.pub` file `pp-client keygen` made. Check the
   fingerprint with the owner. An account may have several keys; choose how many must sign.
3. Its **domain**, if it has one now: 4 to 7 letters (`ACME`), its token's symbol and the name it is
   found by. It is given once and never changed; the app checks that no account has it.
4. Type what it starts with (paid from the registrar's balance, shown at the top) and the fee. The
   smallest fee the chain takes is filled in, including its fee for new accounts.
5. *Make the operation…*, sign, collect the other registrar holders' signatures, submit.

An account created without a domain gets one later, under **Give an account its domain**: its number
and the domain, then the same signing.

**Choosing domains** is the registrar's responsibility: one to three letters are the system accounts';
check that an organization asking for a symbol (a company's ticker) is who it says. When an account
ends, its domain is free again — whether to give it out again, and to whom, is the registrar's call.

Tell the owner their account's number once it is in a block. A token issuer's account is one of
these: its owners run it with the Token app ([Token](token.md)); fund it enough for its renewals.
