# The Registrar app

The registrar creates the accounts the chain issues (numbers from 1,048,576 to 1,073,741,823) and
funds them.

1. Under **New account**, type the account's number, or *Pick a free one*.
2. Add its owner's key: their key card, or the `.pub` file `pp-client keygen` made. Check the
   fingerprint with the owner. An account may have several keys; choose how many must sign.
3. Type what it starts with (paid from the registrar's balance, shown at the top) and the fee. The
   smallest fee the chain takes is filled in, including its fee for new accounts.
4. *Make the operation…*, sign, collect the other registrar holders' signatures, submit.

Tell the owner their account's number once it is in a block. A token issuer's account is one of
these: its owners run it with the Token app ([Token](token.md)); fund it enough for its renewals.
