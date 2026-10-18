# Reviewing and signing

Operations that need several people's signatures travel as files (`.json`): one person makes it,
each signer signs their own copy, and the person who made it combines the copies and submits.
Nobody waits for anybody.

## Sign an operation

1. Open **PP Ledger Signer**, unlock your vault, and *Open an operation file…*.
2. **Read what it does.** The app works this out from the transaction itself, so it is what will
   happen. Check:
   - what it does: the amount, who pays, who receives, the settings it changes;
   - **the deadline** ("Sign by …"): after it, the operation can no longer go through and must be
     made again;
   - **who must sign**, and who has signed. People are named from your address book.
3. **The notes** are the words of whoever made the operation. They may help, but what the operation
   does is shown above them. If the two disagree, do not sign: ask.
4. If one of your keys is asked to sign, press *Sign and save a copy…* and save the copy. Send that
   file back to whoever collects the signatures, by any means (USB stick, email, chat). It holds
   no secret.

The app refuses a file carrying a signature by someone who is not a signer. Such a file was changed,
or mixed up with another: ask for a fresh copy.

## Combine signed copies

Whoever collects the signatures opens one copy, presses *Add signed copies…* and picks the others,
then *Save the merged operation…*. Once there are enough signatures, it says so. A copy of a
different operation is refused.

## Submit (role apps)

In the Reserve, Admin, Registrar or Genesis app, a complete operation shows *Submit to the chain*.
The app sends it, then watches until it is in a block ("In a block: done"). Sending again is
harmless.
