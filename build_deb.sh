#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_DIR="${SCRIPT_DIR}/pkg_root"
VERSION="1.0-1"
ARCH="amd64"
DEB_NAME="freearc_${VERSION}_${ARCH}.deb"

echo "=== Building FreeArc Native from source ==="
cd "${SCRIPT_DIR}"
make clean
make -j$(nproc) all

echo "=== Running test suite before packaging ==="
make test

echo "=== Packaging DEB ==="
rm -rf "${PKG_DIR}"
mkdir -p "${PKG_DIR}/usr/bin"
mkdir -p "${PKG_DIR}/usr/lib/freearc"
mkdir -p "${PKG_DIR}/usr/lib/arc"
mkdir -p "${PKG_DIR}/usr/share/man/man1"
mkdir -p "${PKG_DIR}/usr/share/doc/freearc"
mkdir -p "${PKG_DIR}/DEBIAN"

# Install binaries (already fully stripped and optimized during build)
cp -f "${SCRIPT_DIR}/build/linux/arc" "${PKG_DIR}/usr/bin/arc"
chmod 755 "${PKG_DIR}/usr/bin/arc"

cp -f "${SCRIPT_DIR}/build/linux/unarc" "${PKG_DIR}/usr/bin/unarc"
chmod 755 "${PKG_DIR}/usr/bin/unarc"

cp -f "${SCRIPT_DIR}/build/linux/arc.sfx" "${PKG_DIR}/usr/lib/freearc/arc.sfx"
chmod 755 "${PKG_DIR}/usr/lib/freearc/arc.sfx"

# Symlink compatibility for /usr/lib/arc/arc.sfx
ln -sfn ../freearc/arc.sfx "${PKG_DIR}/usr/lib/arc/arc.sfx"

# Man pages
if [ -f "${SCRIPT_DIR}/man/arc.1" ]; then
    gzip -9c "${SCRIPT_DIR}/man/arc.1" > "${PKG_DIR}/usr/share/man/man1/arc.1.gz"
fi
if [ -f "${SCRIPT_DIR}/man/unarc.1" ]; then
    gzip -9c "${SCRIPT_DIR}/man/unarc.1" > "${PKG_DIR}/usr/share/man/man1/unarc.1.gz"
fi

# Documentation
if [ -f "${SCRIPT_DIR}/README.md" ]; then
    cp "${SCRIPT_DIR}/README.md" "${PKG_DIR}/usr/share/doc/freearc/"
fi
if [ -f "${SCRIPT_DIR}/LICENSE" ]; then
    cp "${SCRIPT_DIR}/LICENSE" "${PKG_DIR}/usr/share/doc/freearc/copyright"
fi

# DEBIAN/control
cat << 'CTRL_EOF' > "${PKG_DIR}/DEBIAN/control"
Package: freearc
Version: 1.0-1
Section: utils
Priority: optional
Architecture: amd64
Maintainer: seb3773 <99963207+seb3773@users.noreply.github.com>
Depends: libc6 (>= 2.15), libstdc++6 (>= 5.2)
Provides: freearc, arc, unarc
Description: Modern native C/C++ archiver, unpacker, and SFX engine
 FreeArc is a high-performance, high-compression archive utility.
 This is a 100% native C/C++ modern rewrite, completely independent
 of Haskell/GHC and Wine, delivering ultra-compact, standalone Linux binaries.
 Features:
  - arc: full archive manager (create, update, freshen, delete, move, SFX conversion)
  - unarc: standalone unpacker, lister, and integrity tester
  - arc.sfx: self-extracting executable stub for autonomous .sfx / .run packages
  - Advanced codecs: LZMA, PPMD, GRZip, Tornado, TTA
  - Smart preprocessors: REP, DICT, DELTA, BCJ/EXE, MM
  - Strong encryption: AES-128/256, Blowfish, Twofish, Serpent
  - Native POSIX permissions, symlinks, directory traversal, and Unix pipes.
CTRL_EOF

# Build DEB
dpkg-deb --build --root-owner-group "${PKG_DIR}" "${SCRIPT_DIR}/${DEB_NAME}"
echo "Package created: ${SCRIPT_DIR}/${DEB_NAME}"

# Clean temp packaging dir
rm -rf "${PKG_DIR}"

echo ""
echo "=== Package Information ==="
dpkg-deb -I "${SCRIPT_DIR}/${DEB_NAME}"
echo ""
echo "=== Package Contents ==="
dpkg-deb -c "${SCRIPT_DIR}/${DEB_NAME}"

echo ""
echo "=== FreeArc packaging finished successfully ==="
