# Network card

What a network's operators publish so that wallets can find it and check it: its id, the hash of its
block 0 — **the anchor the Wallet checks everything back to** ([guides/wallet.md](guides/wallet.md)) —
and a few nodes to ask. Code: `src/core/WalletBook.h`.

UTF-8 JSON, one object:

| Field | Holds |
|-------|-------|
| `format` | `pp-ledger-network-card` |
| `version` | `1`; a reader refuses any other |
| `networkId` | The network's id (as its genesis config names it) |
| `genesisHash` | Block 0's hash, hex (64 digits; `0x` allowed) |
| `nodes` | Node addresses (relays, the beacon): ADP multiaddrs, `/ip4/…` or `/dns4/…` |
| `notes` | Free text from the operators |

```json
{"format": "pp-ledger-network-card", "version": 1, "networkId": "pp-main",
 "genesisHash": "981f509600a16c2081a2f112d2cf0d2f91bf3a0dffc3b5849d86f14597aa71a1",
 "nodes": ["/dns4/relay1.example.org/udp/8519/adp/1.0.0/p2p/Qm…"], "notes": "published 2026-10"}
```

**Publishing one:** the hash is block 0's (`pp-client -b --host <node> block 0` prints it). Publish
the card where people can check it, and the hash in a second place (a site, a printed notice): a card
whose hash was swapped points at another chain, and the hash is what a person checks. Nodes can change
in later cards; the hash never does.
