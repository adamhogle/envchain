#!/usr/bin/env sh
set -eu

if [ "${1:-}" = "" ]; then
  echo "usage: $0 <version>" >&2
  exit 2
fi

VERSION="$1"
ARCH="$(dpkg --print-architecture)"
PKGROOT="$(mktemp -d)"
OUTDIR="dist"
PKGNAME="envchain_${VERSION}_${ARCH}.deb"

trap 'rm -rf "$PKGROOT"' EXIT

mkdir -p "$OUTDIR"
install -D -m755 envchain "$PKGROOT/usr/bin/envchain"
install -D -m644 LICENSE "$PKGROOT/usr/share/doc/envchain/copyright"
mkdir -p "$PKGROOT/DEBIAN"

cat > "$PKGROOT/DEBIAN/control" <<EOF
Package: envchain
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Maintainer: Adam H. Ogle <adam.h.ogle@gmail.com>
Depends: libc6, libsecret-1-0, libreadline8
Description: Secure environment variable loader backed by credential stores
 envchain loads secrets from the system credential store and injects them
 as environment variables for the command it executes.
EOF

dpkg-deb --root-owner-group --build "$PKGROOT" "$OUTDIR/$PKGNAME"
echo "$OUTDIR/$PKGNAME"
