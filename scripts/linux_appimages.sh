#!/usr/bin/env bash
# An AppImage per app, from an install tree (cmake --install <build> --prefix <root>/usr):
#   <out>/PP_Ledger_<App>-<version>-x86_64.AppImage
# Usage: linux_appimages.sh <install root (holds usr/)> <version> <out dir> [appimagetool]
# appimagetool: a path, or downloaded (continuous) when omitted.
set -euo pipefail
root=$(cd "$1" && pwd)
version=$2
out=$3
tool=${4:-}
mkdir -p "$out"
out=$(cd "$out" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

if [[ -z "$tool" ]]; then
  tool="$work/appimagetool"
  curl -fsSL -o "$tool" \
    https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
  chmod +x "$tool"
fi
export APPIMAGE_EXTRACT_AND_RUN=1 # no FUSE needed (CI)

for desktop in "$root"/usr/share/applications/pp-ledger-apps-*.desktop; do
  slug=$(basename "$desktop" .desktop)
  slug=${slug#pp-ledger-apps-}
  exe=$(sed -n 's/^Exec=\([^ ]*\).*/\1/p' "$desktop")
  title=$(sed -n 's/^Name=//p' "$desktop")
  dir="$work/$slug.AppDir"
  mkdir -p "$dir/usr/bin" "$dir/usr/share/pp-ledger-apps"
  cp "$root/usr/bin/$exe" "$dir/usr/bin/"
  cp -r "$root/usr/share/pp-ledger-apps/assets" "$dir/usr/share/pp-ledger-apps/"
  cp "$desktop" "$dir/"
  cp "$root/usr/share/icons/hicolor/512x512/apps/pp-ledger-apps-$slug.png" "$dir/"
  ln -s "usr/bin/$exe" "$dir/AppRun"
  name="${title// /_}-${version}-x86_64.AppImage"
  ARCH=x86_64 "$tool" --no-appstream "$dir" "$out/$name" >"$work/$slug.log" 2>&1 || { cat "$work/$slug.log"; exit 1; }
  echo "$out/$name"
done
