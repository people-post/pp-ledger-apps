#!/usr/bin/env bash
# Chain smoke (development builds, PP_ADMIN_DEV_HOOKS=ON): drive pp-admin-reserve against pp-ledger's
# local smoke network — set the node, make a reserve → registrar transfer, sign it with the smoke
# reserve keys, submit it — and check the registrar's balance on chain.
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

balance() { # the registrar's native balance
  "$client" -b --host "$node" account 2 2>/dev/null | tr -d ' \n' | sed -n 's/.*"mBalances":{"0":\(-\{0,1\}[0-9]*\).*/\1/p'
}
before=$(balance)

rm -rf "$out/home"
"$build/src/apps/devfixture/pp-admin-devfixture" import "$out/home" "reserve passphrase" reserve \
  "$smoke/keys/reserve1.key" "$smoke/keys/reserve2.key" "$smoke/keys/reserve3.key"
sed -e "s|@OUT@|$out|g" -e "s|@NODE@|$node|g" -e "s|@AMOUNT@|$amount|g" \
  "$here/tests/ui/reserve-chain.script" >"$out/reserve-chain.script"
PP_LEDGER_ADMIN_HOME="$out/home" timeout 300 xvfb-run -a -s "-screen 0 1280x900x24" \
  "$build/src/apps/reserve/pp-admin-reserve" --script "$out/reserve-chain.script" >"$out/reserve.log" 2>&1 ||
  { grep -a "script:" "$out/reserve.log"; echo "FAIL: the Reserve app run"; exit 1; }

after=$(balance)
test "$after" -eq $((before + amount)) || { echo "FAIL: registrar balance $before → $after, expected +$amount"; exit 1; }
echo "Chain smoke passed: the registrar received $amount (screenshots in $out)"
