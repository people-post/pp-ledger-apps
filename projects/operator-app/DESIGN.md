# Operator app — design

**Status:** design (op0). Phases: [PHASES.md](PHASES.md) · decisions: [DECISIONS.md](DECISIONS.md) ·
status today: [CURRENT_STATE.md](CURRENT_STATE.md).

A desktop app for the people who **certify the services they run**: a pp-browser relay, a pp-node (with or without
its built-in ledger relay) and a pp-ledger relay run alone. Each service has its own **operator set** (keys and a
threshold, held offline) and its own **certificates**, signed by that set, binding the hot keys the service runs with.
Holders sign in the same Signer, from the same vault, as every pp-ledger operation; the app is built on the apps'
code (OP001). However many services a holder signs for, managing one looks like managing any other.

## Who uses it, for what

| Service kind | Its operator set certifies | Format | Who publishes it |
|--------------|----------------------------|--------|------------------|
| **Relay** (pp-relay-apis: the org's relays per environment, a user-run relay) | the relay's transport key (its PeerId), its signing key, 1–4 origins | pp-browser [SIGNED_FORMATS § 3](https://github.com/people-post/pp-browser/blob/main/projects/release-compatibility/SIGNED_FORMATS.md#3-the-relay-operator-set-and-certificate): frozen for release 1, built | the relay's secrets; pp-browser's `config/relay-certs/<env>.cert` and `PP_RELAY_ID_<ENV>` |
| **Node** (pp-node; `ledger_relay` among its roles when it runs `--ledger-role relay`) | the node's transport key, its account key, its roles | **to be specified** by pp-browser (with its signed node records, N031) | the node, by `install` |
| **Ledger relay** (pp-ledger's `pp-relay`, alone) | the relay's Amp key, its network | **to be specified** by pp-ledger | the relay, by `install` |

A pp-node that runs the ledger relay is **one** node service: it runs the ledger relay on the node's own identity
(pp-browser `docs/ops/BUILD.md`, "Ledger role"), so one certificate names both. A `pp-relay` run alone is a ledger relay service.

The people are those of the relay operators' tool today:

- **Holders** each create their key, hand out its key card, and later review and sign. They use the Signer only.
- **A coordinator** (one of the holders) makes the set, prepares each certificate, merges the signatures, checks the
  result and publishes it. They use the operator app.

Holders never see a seed or a PeerId; the coordinator never holds anyone's key.

## A service

A **service** is the unit every screen works on, whatever its kind:

| Field | Holds |
|-------|-------|
| kind | `relay`, `node` or `ledger-relay` |
| name, environment | as people say it: "Org relay — prod", "Seed node — Frankfurt" |
| operator set | its holders (from their key cards: names, not key hex), its threshold; the set's id (for a relay, its **relay id**) |
| certificate in force | the last one combined and checked, with its sequence and expiry |
| signing log | every body each holder of this vault signed for it: kind, sequence, digest (OP005) |
| kind fields | a relay's origins; a node's roles; a ledger relay's network id and genesis hash |

The coordinator's vault keeps the list of services (`services.json`, beside the address book). A holder's vault knows
a service only through its own key: a key carries the role **Operator** and the service it belongs to, and is never
added to a second service (OP004).

## The screens

The same six for every kind. Only the fields marked *kind* differ.

| Screen | What it does |
|--------|--------------|
| **Services** | The list: kind, name, environment, current sequence, expiry ("none" for release-1 relays), and a warning when a certificate is near its expiry or a publish step is outstanding |
| **New service** | The key-card ceremony the Genesis app already runs: collect the holders' cards, choose the threshold, see the set's id; every holder computes the same id from the same cards and compares it over a second channel |
| **Next certificate** | A form filled in from the certificate in force: sequence + 1, the same keys and *kind* fields; the coordinator changes what is changing (a new transport key from the host, an origin, a role, a later expiry). The result is an operation file, handed to the holders |
| **Review** (the Signer's screen) | What the certificate says, **and what changes** against the one in force: "sequence 3 → 4; transport PeerId changed (Qm…a1 → Qm…7f); origins unchanged; no expiry". Signing refuses a second body at a sequence this key already signed |
| **Merge and check** | Combine the holders' copies, verify the certificate as its readers do (set, threshold, distinct signers, sequence above the one in force, *kind* rules), and keep it as the certificate in force |
| **Publish** | *Kind*: what to put where, each item copied or saved as a file (OP006) |

Maintenance is the same three flows for every kind, each a guided run of the screens above:

- **Rotate a host key** (a host move, a leak): the host makes its new keys and hands over their public keys; next
  certificate; publish. For a relay, the 24 h window in which the old PeerId stays accepted (SIGNED_FORMATS Q11) is
  shown, and never offered after a compromise.
- **Renew** (kinds with an expiry): the same body with a later expiry and the next sequence; the Services list warns
  from 14 days before.
- **Replace a holder's key** (lost or leaked): an operator-set change signed by the set in force, then a certificate
  that carries it. The service's id does not change. Losing more keys than the threshold allows makes a new service.

### Publish, by kind

| Kind | Publish gives |
|------|---------------|
| Relay | the certificate's base64 for the relay's certificate secret; the relay id for the relay's configuration and pp-browser's `PP_RELAY_ID_<ENV>`; the `.cert` file text for pp-browser's `config/relay-certs/<env>.cert`, commented as today's files are; the relay link `https://host/api/relay#relay:…` |
| Node | the certificate file for `pp-node operator install` |
| Ledger relay | the certificate file for `pp-relay operator install` |

Publishing changes nothing outside the app; it lists the steps and checks them off as the coordinator confirms each.
Checking what a host serves against the certificate in force (a relay's `GET /v1/relay-identity`, a ledger relay's
`STATUS`) comes later (op4).

## The certificate operation file

Certificates are not chain transactions, so they do not travel in pp-ledger's operation file (its signed bytes are
`record.signingMessage(networkId)`). They travel in a sibling with the same shape (OP002), so the Signer's open, review,
sign and merge are one path:

| Field | Holds |
|-------|-------|
| `format`, `version` | `pp-service-certificate-operation`, 1 |
| `kind` | `relay-certificate`, `relay-operator-set-change`, `node-certificate`, `ledger-relay-certificate`, … |
| `service` | the service's id (a relay id for a relay) and its name, as the coordinator wrote it |
| `body` | the exact bytes signed (base64): a certificate body or a set-change body |
| `signers` | the operator set in force: `threshold`, `publicKeys` in the set's order |
| `signatures` | `{index, signature}` collected, in any order |
| `notes`, `createdAt` | the coordinator's text; when it was made |

The rules are the admin apps': **the meaning comes from the bytes** (what Review shows is decoded from `body`, never from
`service` or `notes`); every signature is checked on opening; any copies of one body merge. There is no deadline: a
certificate body does not expire before it is signed. A body carries its own expiry, if its kind has one.

## Keys

- **One key per holder per service,** never reused across services or environments (pp-relay-apis' rule today). The
  vault's key gains the role `Operator` and the service it is for; Review shows "Operator of Org relay — prod".
- **The host's hot keys are not operator keys.** The app never makes or holds a relay's transport or signing seed, or a
  node's identity. It takes their **public** keys, from `relay-key` output, `pp-node operator request` or
  `pp-relay operator request`, and refuses one that is also an operator key of the set (SIGNED_FORMATS § 3.2).
- **Keys from today's tool import into the vault.** A holder's `relay-operator.mjs keygen` file (the seed, mode 0600)
  imports once into the encrypted vault, and the app says to delete the plain file and keep a vault backup. The relay
  id is unchanged.

## Shape of the code

| Piece | What |
|-------|------|
| `apps_core` | `Services` (the list, the certificate in force, the signing log); `CertificateOperation` (the file: open, verify signatures, sign, merge); the kind interface: decode a body for Review, the changes against the certificate in force, verify, the next body from a form, the publish items |
| `apps_ui` | the Services, New service, Next certificate, Merge and check and Publish screens; Review shared with the Signer |
| `pp-ledger-operator` | the operator app: core and UI; network only from op4 |
| `pp-ledger-signer` | opens certificate operation files beside pp-ledger's (still no network code) |
| the codec | the relay certificate's, from a shared library (OP003); a kind's codec comes from its format's owner |

## What it does not do

- Run or deploy anything: setting a relay's secrets, editing pp-browser's CI variables, installing on a node. It lists
  the steps; people with that access do them.
- Define the node or ledger relay certificate: those formats are pp-browser's and pp-ledger's. The app has their kinds
  once the formats exist (op5).
- Hold hot keys, or keys on hardware tokens (AD010).
- Stand in for `relay-operator.mjs` before it is built and tested against the frozen vectors: release 1's staging and
  prod ceremonies use the tool.
