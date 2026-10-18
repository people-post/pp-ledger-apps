# Rebuilding a release

Public source proves little if nobody can tie a download to it (projects/wallet-app WL007). Release
builds set `PP_APPS_RELEASE`: no path of the build machine goes into the binaries — the source and build
trees are mapped to `.` and `build` (`-ffile-prefix-map`), and the development fallback to the source
tree's assets is left out. The same source, built with the same compiler and libraries, gives the same
program, byte for byte, wherever it is built.

## Check a Linux release

On Ubuntu 24.04 (the release's builder: `ubuntu-24.04`, its GCC):

```bash
git clone https://github.com/people-post/pp-ledger-apps && cd pp-ledger-apps
git checkout v1.2.3            # the release's tag
sudo apt-get install -y ninja-build libx11-dev libxext-dev libxcursor-dev libxinerama-dev libxi-dev \
  libxrandr-dev libxfixes-dev libxrender-dev libxss-dev libgl-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPP_APPS_BUILD_TESTS=OFF -DPP_APPS_RELEASE=ON \
  -DPP_APPS_VERSION=1.2.3
cmake --build build --target pp-ledger-wallet
sha256sum build/src/apps/wallet/pp-ledger-wallet
```

and compare with the program in the release's package — **stripped**, as packages ship it:

```bash
dpkg-deb -x pp-ledger-wallet_1.2.3_amd64.deb pkg && sha256sum pkg/usr/bin/pp-ledger-wallet
strip -o built-stripped build/src/apps/wallet/pp-ledger-wallet && sha256sum built-stripped
```

## What is, and is not, reproducible

- **The programs** are: checked by building the wallet twice, from different directories, and
  comparing the bytes (wl6).
- **The packages around them** are not, byte for byte: a `.deb`, an AppImage, a disk image and an
  installer carry the time they were made. Compare the programs inside them; `SHA256SUMS` in each
  release ties the packages themselves to the release.
- **macOS**: signed bundles carry Apple's signature and timestamp; compare after
  `codesign --remove-signature`.
- **Windows**: release builds ask MSVC for deterministic output (`/Brepro`, the debug file named
  without its path); not yet checked by rebuilding — the Windows build is new.
