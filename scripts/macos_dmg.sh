#!/usr/bin/env bash
# The release disk image: the apps' bundles from an install folder (signed or not) and an
# Applications shortcut to drag them to. Prints the .dmg's path.
# Usage: macos_dmg.sh <install folder holding the .app bundles> <version> <out dir>
set -euo pipefail
src=$(cd "$1" && pwd)
version=$2
mkdir -p "$3"
out=$(cd "$3" && pwd)
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
cp -R "$src"/*.app "$stage/"
ln -s /Applications "$stage/Applications"
dmg="$out/pp-ledger-admin-${version}-macos-$(uname -m).dmg"
rm -f "$dmg"
hdiutil create -volname "PP Ledger Admin" -srcfolder "$stage" -ov -format UDZO "$dmg" >/dev/null
echo "$dmg"
