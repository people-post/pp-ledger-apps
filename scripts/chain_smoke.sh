#!/usr/bin/env bash
# Chain smoke (development builds, PP_APPS_DEV_HOOKS=ON): drive the role apps against pp-ledger's
# local smoke network and check the chain after —
#   pp-ledger-reserve: a reserve → registrar transfer (the registrar's balance);
#   pp-ledger-admin: a config update raising the mint cap (the config in force), a mint to the
#   reserve (the reserve's balance), a rotation of the admin set (2 of 4);
#   pp-ledger-registrar: an issued account for an owner key, funded by the registrar (the account);
#   pp-ledger-token: that account as a token issuer — issues to the registrar (both balances of its
#   token), replaces its signers (2 of 2), sends native tokens signed by both (the registrar's balance);
#   pp-ledger-fees: the first fee round — the period from the chain, a relay paid to that account,
#   the payouts as one batch from the fee account, signed 3 of 3 (balances, the round file, the mark);
#   pp-ledger-wallet: the network pinned from a card, accounts proven back to block 0's hash; an account
#   given a recovery set, its recovery announced from a second wallet holding the set, and cancelled;
#   pp-ledger-genesis: a recovery announcing a new admin set (the authority's pending recovery) — last,
#   as it freezes the admin set until it applies.
# Each made from the chain, signed with the smoke keys (3 of 3), submitted, seen in a block.
# Usage: chain_smoke.sh <build dir> <pp-ledger checkout, built with its apps> [<output dir>]
set -euo pipefail
build=$(cd "$1" && pwd)
ledger=$(cd "$2" && pwd)
out=${3:-$(mktemp -d)}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
client="$ledger/build/app/pp-client"
smoke="$ledger/build/test-smoke"
amount=$((50000 + RANDOM))
mkdir -p "$out"

"$ledger/scripts/test/pp_ledger_network.sh" stop >/dev/null 2>&1 || true
"$ledger/scripts/test/pp_ledger_network.sh" up >"$out/network.log" 2>&1 || { echo "FAIL: network up"; exit 1; }
trap '"$ledger/scripts/test/pp_ledger_network.sh" stop >/dev/null 2>&1 || true' EXIT
node=$(cat "$smoke/relay/listen.multiaddr")

balance() { # account $1's native balance
  "$client" -b --host "$node" account "$1" 2>/dev/null | tr -d ' \n' | sed -n 's/.*"mBalances":{"0":\(-\{0,1\}[0-9]*\).*/\1/p'
}
run_app() { # <app> <home> <script>
  local app=$1 home=$2 script=$3
  sed -e "s|@OUT@|$out|g" -e "s|@NODE@|$node|g" -e "s|@AMOUNT@|$amount|g" -e "s|@MINT@|$mint|g" -e "s|@CAP@|$cap|g" \
    -e "s|@ACCOUNT@|$account|g" -e "s|@FUND@|$fund|g" -e "s|@ISSUE@|$issue|g" -e "s|@SEND@|$send|g" \
    "$here/tests/ui/$script" >"$out/$script"
  PP_LEDGER_APPS_HOME="$home" timeout 300 xvfb-run -a -s "-screen 0 1280x900x24" \
    "$build/src/apps/$app/pp-ledger-$app" --script "$out/$script" >"$out/$app.log" 2>&1 ||
    { grep -a "script:" "$out/$app.log"; echo "FAIL: the $app app run"; exit 1; }
}
mint=$((5000 + RANDOM))
account=$(((1 << 20) + 1000 + RANDOM))
fund=$((20000 + RANDOM % 900))
issue=$((1000 + RANDOM))
send=$((10 + RANDOM % 90))
cap=$((700000 + RANDOM))

# 1. Reserve: a transfer to the registrar.
before=$(balance 2)

rm -rf "$out/home"
"$build/src/apps/devfixture/pp-ledger-devfixture" import "$out/home" "reserve passphrase" reserve \
  "$smoke/keys/reserve1.key" "$smoke/keys/reserve2.key" "$smoke/keys/reserve3.key"
run_app reserve "$out/home" reserve-chain.script
after=$(balance 2)
test "$after" -eq $((before + amount)) || { echo "FAIL: registrar balance $before → $after, expected +$amount"; exit 1; }
echo "✓ Reserve: the registrar received $amount"

# 2. Admin: a config update raising the mint cap, then a mint.
reserveBefore=$(balance 1)
rm -rf "$out/admin-home" "$out/admin-new".*
"$client" keygen -o "$out/admin-new" >/dev/null # the fourth admin of the rotation
"$build/src/apps/devfixture/pp-ledger-devfixture" import "$out/admin-home" "admin passphrase" admin \
  "$smoke/keys/genesis1.key" "$smoke/keys/genesis2.key" "$smoke/keys/genesis3.key" "$out/admin-new.key"
run_app admin "$out/admin-home" admin-chain.script
reserveAfter=$(balance 1)
test "$reserveAfter" -eq $((reserveBefore + mint)) ||
  { echo "FAIL: reserve balance $reserveBefore → $reserveAfter, expected +$mint"; exit 1; }
"$client" -b --host "$node" config 2>/dev/null | grep -q "^Config in force:.*maxIssuancePerEpoch: $cap," ||
  { echo "FAIL: the config in force does not carry maxIssuancePerEpoch $cap"; exit 1; }
echo "✓ Admin: the mint cap is $cap"
echo "✓ Admin: minted $mint to the reserve"
"$client" -b --host "$node" config 2>/dev/null | grep -q "^Admin keys: 2 of 4:" ||
  { echo "FAIL: the admin set is not 2 of 4"; exit 1; }
echo "✓ Admin: the admin set is 2 of 4"

# 3. Registrar: an issued account for an owner, funded by the registrar.
rm -rf "$out/registrar-home" "$out/owner".*
"$client" keygen -o "$out/owner" >/dev/null
"$build/src/apps/devfixture/pp-ledger-devfixture" import "$out/registrar-home" "registrar passphrase" registrar \
  "$smoke/keys/registrar1.key" "$smoke/keys/registrar2.key" "$smoke/keys/registrar3.key"
run_app registrar "$out/registrar-home" registrar-chain.script
# Renewals take their fee (a few units on the smoke chain) from it from then on.
held=$(balance "$account")
test -n "$held" && test "$held" -le "$fund" && test "$held" -gt $((fund - 100)) ||
  { echo "FAIL: account $account holds ${held:-nothing}, expected $fund less renewals"; exit 1; }
"$client" -b --host "$node" account "$account" 2>/dev/null | tr -d ' \n' | grep -qi "$(tr -d ' \n' <"$out/owner.pub")" ||
  { echo "FAIL: account $account is not the owner's key"; exit 1; }
echo "✓ Registrar: account $account created for the owner's key, holding $held of $fund"

# 4. Token: the new account issues its token, replaces its signers, sends from them both.
rm -rf "$out/token-home" "$out/owner2".*
"$client" keygen -o "$out/owner2" >/dev/null
"$build/src/apps/devfixture/pp-ledger-devfixture" import "$out/token-home" "token passphrase" owner \
  "$out/owner.key" "$out/owner2.key"
registrarBefore=$(balance 2)
run_app token "$out/token-home" token-chain.script
account_json() { "$client" -b --host "$node" account "$1" 2>/dev/null | tr -d ' \n'; }
account_json 2 | grep -q "\"$account\":$issue[,}]" || { echo "FAIL: the registrar does not hold $issue of token $account"; exit 1; }
account_json "$account" | grep -q "\"$account\":-$issue[,}]" || { echo "FAIL: token $account's supply is not $issue"; exit 1; }
account_json "$account" | grep -q '"minSignatures":2' || { echo "FAIL: account $account is not 2 of 2"; exit 1; }
test "$(balance 2)" -eq $((registrarBefore + send)) ||
  { echo "FAIL: registrar balance $registrarBefore → $(balance 2), expected +$send"; exit 1; }
echo "✓ Token: account $account issued $issue to the registrar, is 2 of 2, sent $send signed by both"

# 5. Fees: the first round. The fee account is topped up from the reserve first (the smoke chain's fees
# are few), with pp-client's operation files.
op_flow() { # <name> <keys…> -- <op args…>: make, sign apart, merge, submit
  local name=$1 keys=() i=0 copies=()
  shift
  while [[ $1 != "--" ]]; do keys+=("$1"); shift; done
  shift
  rm -f "$out/$name".*
  "$client" -b --host "$node" op "$@" -o "$out/$name.json" >/dev/null || { echo "FAIL: op $1 ($name)"; exit 1; }
  for key in "${keys[@]}"; do
    i=$((i + 1))
    cp "$out/$name.json" "$out/$name.copy$i.json"
    "$client" op sign "$out/$name.copy$i.json" -k "$key" >/dev/null || { echo "FAIL: op sign ($name)"; exit 1; }
    copies+=("$out/$name.copy$i.json")
  done
  "$client" op merge "${copies[@]}" -o "$out/$name.signed.json" >/dev/null || { echo "FAIL: op merge ($name)"; exit 1; }
  "$client" -b --host "$node" op submit "$out/$name.signed.json" >/dev/null || { echo "FAIL: op submit ($name)"; exit 1; }
}
feeBefore=$(balance 3)
op_flow fee-top-up "$smoke/keys/reserve1.key" "$smoke/keys/reserve2.key" "$smoke/keys/reserve3.key" -- \
  transfer --from 1 --to 3 --amount 200000 -f 1
for _ in $(seq 60); do
  [[ "$(balance 3)" -ge $((feeBefore + 200000)) ]] && break
  sleep 1
done
[[ "$(balance 3)" -ge $((feeBefore + 200000)) ]] || { echo "FAIL: the fee account was not topped up"; exit 1; }
rm -rf "$out/fees-home"
"$build/src/apps/devfixture/pp-ledger-devfixture" import "$out/fees-home" "fees passphrase" fee \
  "$smoke/keys/fee1.key" "$smoke/keys/fee2.key" "$smoke/keys/fee3.key"
relayBefore=$(balance "$account")
feeBefore=$(balance 3)
run_app fees "$out/fees-home" fees-chain.script
test -f "$out/payouts.round.json" || { echo "FAIL: no round file beside the payouts"; exit 1; }
relayPaid=$(tr -d ' \n' <"$out/payouts.round.json" | grep -o "\"account\":$account,\"who\":\"relay-smoke\",\"amount\":[0-9]*" |
  sed 's/.*"amount"://')
test -n "$relayPaid" && test "$relayPaid" -gt 0 || { echo "FAIL: the round pays the relay nothing"; exit 1; }
relayAfter=$(balance "$account")
# The relay's account renews meanwhile (a fee or two on the smoke chain).
test "$relayAfter" -gt $((relayBefore + relayPaid - 10)) ||
  { echo "FAIL: relay account $account $relayBefore → $relayAfter, expected +$relayPaid"; exit 1; }
total=$(tr -d ' \n' <"$out/payouts.round.json" | sed -n 's/.*"total":\([0-9]*\),"fees".*/\1/p')
# Fees keep coming in meanwhile (a few units each on the smoke chain).
test "$(balance 3)" -le $((feeBefore - total + 1000)) ||
  { echo "FAIL: the fee account paid less than the round's $total"; exit 1; }
"$client" -b --host "$node" transactions 3 >"$out/fee-transactions.json" 2>&1 || true
grep -q "pp-fee-round/1 1 " "$out/fee-transactions.json" ||
  { echo "FAIL: the payouts do not carry the round's mark ($out/fee-transactions.json)"; exit 1; }
echo "✓ Fees: round 1 paid $total (the relay $relayPaid), one batch signed 3 of 3, each payout marked"

# 6. Wallet: the network pinned from a card (block 0's hash), accounts proven back to it.
genesisHash=$("$client" -b --host "$node" block 0 2>/dev/null | tr -d ' \n' | sed -n 's/^{"hash":"0x\([0-9a-f]*\)".*/\1/p')
networkId=$("$client" -b --host "$node" status 2>/dev/null | tr -d ' \n' | sed -n 's/.*"networkId":"\([^"]*\)".*/\1/p')
test -n "$genesisHash" && test -n "$networkId" || { echo "FAIL: cannot read block 0's hash or the network id"; exit 1; }
cat >"$out/network-card.json" <<CARD
{"format": "pp-ledger-network-card", "version": 1, "networkId": "$networkId", "genesisHash": "$genesisHash",
 "nodes": ["$node"], "notes": "the chain smoke"}
CARD
rm -rf "$out/wallet-home"
"$build/src/apps/devfixture/pp-ledger-devfixture" import "$out/wallet-home" "wallet passphrase" owner \
  "$out/owner.key" "$out/owner2.key"
registrarBefore=$(balance 2)
run_app wallet "$out/wallet-home" wallet-chain.script
echo "✓ Wallet: network $networkId pinned from its card; accounts $account and 2 proven, an absent one proven absent"
test "$(balance 2)" -eq $((registrarBefore + 5)) ||
  { echo "FAIL: registrar balance $registrarBefore → $(balance 2), expected +5 from the wallet"; exit 1; }
echo "✓ Wallet: sent 5 from account $account to the registrar, signed with its two keys"
"$client" -b --host "$node" account "$account" 2>/dev/null | tr -d ' \n' >"$out/evacuated.json"
grep -q '"minSignatures":1' "$out/evacuated.json" && test "$(grep -o '"publicKeys":\[[^]]*\]' "$out/evacuated.json" | grep -o '"0x' | wc -l)" -eq 1 ||
  { echo "FAIL: account $account's keys were not replaced by one new key ($out/evacuated.json)"; exit 1; }
echo "✓ Wallet: account $account evacuated — its keys replaced by a new one made in the wallet"

# 6b. Wallet: account recovery — a recovery set (2 of 2, a day to cancel in), an announcement from a
# second wallet holding the set, cancelled by the account's key.
for i in 1 2; do
  "$client" keygen -o "$out/guardian$i" >/dev/null
  rm -f "$out/guardian$i.card.json"
  "$build/src/apps/devfixture/pp-ledger-devfixture" card-file owner "Guardian $i" "$out/guardian$i.pub" \
    "$out/guardian$i.card.json"
done
account_json() { "$client" -b --host "$node" account "$account" 2>/dev/null | tr -d ' \n'; }
run_app wallet "$out/wallet-home" wallet-recovery-set.script
account_json >"$out/recovery-set.json"
grep -q '"recovery":{[^}]*"minSignatures":2' "$out/recovery-set.json" ||
  { echo "FAIL: account $account has no recovery set of 2 ($out/recovery-set.json)"; exit 1; }
echo "✓ Wallet: account $account has a recovery set (2 of 2) and a day's delay"
rm -rf "$out/guardian-home"
"$build/src/apps/devfixture/pp-ledger-devfixture" import "$out/guardian-home" "guardian passphrase" owner \
  "$out/guardian1.key" "$out/guardian2.key"
run_app wallet "$out/guardian-home" wallet-recover.script
account_json >"$out/recovery-announced.json"
grep -q '"pending":{' "$out/recovery-announced.json" ||
  { echo "FAIL: no recovery announced for account $account ($out/recovery-announced.json)"; exit 1; }
echo "✓ Wallet: account $account's recovery set announced a new key"
run_app wallet "$out/wallet-home" wallet-recovery-cancel.script
account_json >"$out/recovery-cancelled.json"
! grep -q '"pending"' "$out/recovery-cancelled.json" && grep -q '"recovery":{' "$out/recovery-cancelled.json" ||
  { echo "FAIL: account $account's recovery was not cancelled ($out/recovery-cancelled.json)"; exit 1; }
echo "✓ Wallet: the owner saw the announcement on Accounts and cancelled it; the recovery set stays"

# 7. Genesis: the recovery set announces a new admin set.
rm -rf "$out/genesis-home" "$out/next-admin"*
for i in 1 2 3; do "$client" keygen -o "$out/next-admin$i" >/dev/null; done
"$build/src/apps/devfixture/pp-ledger-devfixture" import "$out/genesis-home" "genesis passphrase" recovery \
  "$smoke/keys/recovery1.key"
"$build/src/apps/devfixture/pp-ledger-devfixture" import "$out/genesis-home" "genesis passphrase" admin \
  "$out/next-admin1.key" "$out/next-admin2.key" "$out/next-admin3.key"
run_app genesis "$out/genesis-home" recovery-chain.script
"$client" -b --host "$node" config 2>/dev/null | grep -q "pendingRecovery: {adminKeys: 3, adminMinSignatures: 2" ||
  { echo "FAIL: no recovery pending for the new admin set"; exit 1; }
echo "✓ Genesis: a recovery for a new admin set (2 of 3) is pending"

echo "Chain smoke passed (screenshots in $out)"
