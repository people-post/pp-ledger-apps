#!/usr/bin/env bash
# UI smoke (development builds, PP_ADMIN_DEV_HOOKS=ON): drive pp-admin-signer under a virtual X
# display — create a vault and a key; unlock a fixture vault, sign its transfer, merge a copy signed
# apart, sign its genesis plan, lock when left alone — and check the results. Usage: ui_smoke.sh <build dir> [<output dir>]
set -euo pipefail
build=$(cd "$1" && pwd)
out=${2:-$(mktemp -d)}
mkdir -p "$out"
signer="$build/src/apps/signer/pp-admin-signer"
fixture="$build/src/apps/devfixture/pp-admin-devfixture"
here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

run() { # <home> <script> [args...]
  local home=$1 script=$2
  shift 2
  sed -e "s|@OUT@|$out|g" -e "s|@FIXTURE@|$out/fixture|g" "$script" >"$out/$(basename "$script")"
  PP_LEDGER_ADMIN_HOME="$home" timeout 180 xvfb-run -a -s "-screen 0 1280x900x24" \
    "$signer" --script "$out/$(basename "$script")" "$@"
}

# 1. A first run: the vault (real Argon2id limits) and a key appear.
rm -rf "$out/fresh" && mkdir -p "$out/fresh"
run "$out/fresh" "$here/tests/ui/create.script"
test -f "$out/fresh/vault/vault.bin" || { echo "FAIL: no vault created"; exit 1; }
test "$(ls "$out/fresh/vault/keys" | wc -l)" -eq 1 || { echo "FAIL: no key made"; exit 1; }

# 2. Signing: the copy saved carries the fixture's signature and ours.
rm -rf "$out/fixture"
"$fixture" "$out/fixture" "fixture passphrase" >/dev/null
run "$out/fixture" "$here/tests/ui/sign.script" "$out/fixture/transfer.json"
"$fixture" verify "$out/transfer.signed.json" 2 || { echo "FAIL: the signed copy is not 2 of 2"; exit 1; }

# 3. Merging: Bob's copy added to Alice's makes 2 of 2.
run "$out/fixture" "$here/tests/ui/merge.script" "$out/fixture/transfer.json"
"$fixture" verify "$out/transfer.merged.json" 2 || { echo "FAIL: the merged copy is not 2 of 2"; exit 1; }

# 4. A genesis plan: our admin key signs every record of block 0 (Alice's signature plus ours).
run "$out/fixture" "$here/tests/ui/genesis.script" "$out/fixture/genesis.json"
"$fixture" verify "$out/genesis.signed.json" 2 || { echo "FAIL: the signed genesis is not 2 of 2"; exit 1; }

# 5. Left alone, the app locks the vault (the idle timeout shortened for the run).
PP_ADMIN_DEV_IDLE_SECONDS=2 run "$out/fixture" "$here/tests/ui/lock.script" ||
  { echo "FAIL: the app did not lock itself"; exit 1; }

echo "UI smoke passed (screenshots in $out)"
