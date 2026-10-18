#!/usr/bin/env bash
# Genesis smoke (development builds, PP_ADMIN_DEV_HOOKS=ON): the offline genesis ceremony with
# pp-admin-genesis, then pp-ledger's beacon starting from its result —
#   each participant's key (pp-client keygen), the beacon's signing key in its work directory;
#   the coordinator's vault: the admin, recovery and account keys, the beacon's and a miner's key
#   cards; the Genesis app plans block 0 and its admin keys sign it 2 of 3;
#   pp-beacon --init --genesis-op writes block 0 from the signed plan.
# Usage: genesis_smoke.sh <build dir> <pp-ledger checkout, built with its apps> [<output dir>]
set -euo pipefail
build=$(cd "$1" && pwd)
ledger=$(cd "$2" && pwd)
out=${3:-$(mktemp -d)}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
client="$ledger/build/app/pp-client"
beacon="$ledger/build/app/pp-beacon"
fixture="$build/src/apps/devfixture/pp-admin-devfixture"
rm -rf "$out" && mkdir -p "$out/keys" "$out/beacon/keys"

for name in admin1 admin2 admin3 recovery1 reserve1 registrar1 fee1 recycle1 miner1; do
  "$client" keygen -o "$out/keys/$name" >/dev/null
done
"$client" keygen -o "$out/beacon/keys/beacon-signing" >/dev/null # the beacon's operator's key

home="$out/home"
pass="genesis passphrase"
"$fixture" import "$home" "$pass" admin "$out/keys/admin1.key" "$out/keys/admin2.key" "$out/keys/admin3.key"
for role in recovery reserve registrar fee recycle; do
  "$fixture" import "$home" "$pass" "$role" "$out/keys/${role}1.key"
done
"$fixture" card "$home" "$pass" beacon "Beacon operator" "$out/beacon/keys/beacon-signing.pub"
"$fixture" card "$home" "$pass" miner "Miner one" "$out/keys/miner1.pub"

sed -e "s|@OUT@|$out|g" "$here/tests/ui/genesis-plan.script" >"$out/genesis-plan.script"
PP_LEDGER_ADMIN_HOME="$home" timeout 300 xvfb-run -a -s "-screen 0 1280x900x24" \
  "$build/src/apps/genesis/pp-admin-genesis" --script "$out/genesis-plan.script" >"$out/genesis.log" 2>&1 ||
  { grep -a "script:" "$out/genesis.log"; echo "FAIL: the Genesis app run"; exit 1; }
"$fixture" verify "$out/genesis.signed2.json" 2 || { echo "FAIL: the plan is not signed 2 of 3"; exit 1; }
echo "✓ Genesis: block 0 planned and signed 2 of 3 by the admins"

"$beacon" -d "$out/beacon" --init --genesis-op "$out/genesis.signed2.json" >"$out/beacon-init.log" 2>&1 ||
  { tail -5 "$out/beacon-init.log"; echo "FAIL: pp-beacon refused the signed plan"; exit 1; }
echo "✓ Beacon: block 0 written from the signed plan"
echo "Genesis smoke passed (screenshots in $out)"
