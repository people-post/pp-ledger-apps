#!/usr/bin/env bash
# A release disk image: the bundles of an install folder (signed or not) and an Applications
# shortcut to drag them to. Prints the .dmg's path.
# Usage: macos_dmg.sh <install folder holding the .app bundles> <version> <out dir> [system|token]
#   system (default): "PP Ledger System", pp-ledger-system-<version>-macos-<arch>.dmg
#   token: "PP Ledger Token", pp-ledger-token-<version>-macos-<arch>.dmg
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
  *) echo "unknown set: $set (system or token)" >&2; exit 1 ;;
esac
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
cp -R "$src"/*.app "$stage/"
ln -s /Applications "$stage/Applications"
dmg="$out/pp-ledger-${set}-${version}-macos-$(uname -m).dmg"
rm -f "$dmg"
hdiutil create -volname "$volume" -srcfolder "$stage" -ov -format UDZO "$dmg" >/dev/null
echo "$dmg"
