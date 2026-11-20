#!/usr/bin/env bash
# A release disk image: the bundles of an install folder (signed or not) and an Applications
# shortcut to drag them to. Prints the .dmg's path.
# Usage: macos_dmg.sh <install folder holding the .app bundles> <version> <out dir> [system|token|wallet]
#   system (default): "PP Ledger System", pp-ledger-system-<version>-macos-<arch>.dmg
#   token: "PP Ledger Token", pp-ledger-token-<version>-macos-<arch>.dmg
#   wallet: "PP Ledger Wallet", pp-ledger-wallet-<version>-macos-<arch>.dmg (the wallet alone)
# The install folder holds one set (cmake --install … --component signer, then system or token).
set -euo pipefail
src=$(cd "$1" && pwd)
version=$2
mkdir -p "$3"
out=$(cd "$3" && pwd)
set=${4:-system}
case "$set" in
  system) volume="PP Ledger System" ;;
  token) volume="PP Ledger Token" ;;
  wallet) volume="PP Ledger Wallet" ;;
  *) echo "unknown set: $set (system, token or wallet)" >&2; exit 1 ;;
esac
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
cp -R "$src"/*.app "$stage/"
ln -s /Applications "$stage/Applications"
dmg="$out/pp-ledger-${set}-${version}-macos-$(uname -m).dmg"
rm -f "$dmg"
# hdiutil occasionally fails with "Resource busy" on GHA macos runners when several
# DMGs are built back-to-back; retry a few times like pp-browser's release packaging.
attempts=0
max_attempts=5
until hdiutil create -volname "$volume" -srcfolder "$stage" -ov -format UDZO "$dmg" >/dev/null; do
  attempts=$((attempts + 1))
  if [[ "$attempts" -ge "$max_attempts" ]]; then
    echo "error: hdiutil create failed after ${max_attempts} attempts: ${dmg}" >&2
    exit 1
  fi
  echo "warning: hdiutil create attempt ${attempts} failed; retrying in 5s" >&2
  rm -f "$dmg"
  sleep 5
done
echo "$dmg"
