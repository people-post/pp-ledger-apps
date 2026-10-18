#!/usr/bin/env bash
# Chain smoke (development builds, PP_ADMIN_DEV_HOOKS=ON): drive the role apps against pp-ledger's
# local smoke network and check the chain after —
#   pp-admin-reserve: a reserve → registrar transfer (the registrar's balance);
#   pp-admin-admin: a config update raising the mint cap (the config in force), a mint to the
#   reserve (the reserve's balance), a rotation of the admin set (2 of 4).
# Each made from the chain, signed with the smoke keys (3 of 3), submitted, seen in a block.
# Usage: chain_smoke.sh <build dir> <pp-ledger checkout, built with its apps> [<output dir>]
set -euo pipefail
build=$(cd "$1" && pwd)
ledger=$(cd "$2" && pwd)
out=${3:-$(mktemp -d)}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
client="$ledger/build/app/pp-client"
smoke="$ledger/build/test-smoke"
amount=$((1000 + RANDOM))
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
    "$here/tests/ui/$script" >"$out/$script"
  PP_LEDGER_ADMIN_HOME="$home" timeout 300 xvfb-run -a -s "-screen 0 1280x900x24" \
    "$build/src/apps/$app/pp-admin-$app" --script "$out/$script" >"$out/$app.log" 2>&1 ||
    { grep -a "script:" "$out/$app.log"; echo "FAIL: the $app app run"; exit 1; }
}
mint=$((5000 + RANDOM))
cap=$((700000 + RANDOM))

# 1. Reserve: a transfer to the registrar.
before=$(balance 2)

rm -rf "$out/home"
"$build/src/apps/devfixture/pp-admin-devfixture" import "$out/home" "reserve passphrase" reserve \
  "$smoke/keys/reserve1.key" "$smoke/keys/reserve2.key" "$smoke/keys/reserve3.key"
run_app reserve "$out/home" reserve-chain.script
after=$(balance 2)
test "$after" -eq $((before + amount)) || { echo "FAIL: registrar balance $before → $after, expected +$amount"; exit 1; }
echo "✓ Reserve: the registrar received $amount"

# 2. Admin: a config update raising the mint cap, then a mint.
reserveBefore=$(balance 1)
rm -rf "$out/admin-home" "$out/admin-new".*
"$client" keygen -o "$out/admin-new" >/dev/null # the fourth admin of the rotation
"$build/src/apps/devfixture/pp-admin-devfixture" import "$out/admin-home" "admin passphrase" admin \
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

echo "Chain smoke passed (screenshots in $out)"
