#!/usr/bin/env bash
# UI smoke (development builds, PP_ADMIN_DEV_HOOKS=ON): drive pp-admin-signer under a virtual X
# display — create a vault and a key; unlock a fixture vault and sign its operation — and check the
# results on disk. Usage: ui_smoke.sh <build dir> [<output dir>]
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
  sed "s|@OUT@|$out|g" "$script" >"$out/$(basename "$script")"
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

echo "UI smoke passed (screenshots in $out)"
