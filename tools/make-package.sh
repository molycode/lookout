#!/bin/bash
# Package dist/lookout, the binary tools/build-release.sh built, as dist/lookout-<version>-x86_64.tar.xz.
#
#   tools/make-package.sh
#
# The version comes from the binary itself, so a package can never carry another build's number.

set -u

if [ -t 1 ]; then
	C_DIM=$(printf '\033[2m'); C_B=$(printf '\033[1m'); C_OFF=$(printf '\033[0m')
	C_OK=$(printf '\033[32m'); C_ERR=$(printf '\033[31m')
else
	C_DIM=""; C_B=""; C_OFF=""; C_OK=""; C_ERR=""
fi
case "${LANG:-}${LC_ALL:-}" in
	*UTF-8*|*utf8*|*UTF8*) G_OK="✓"; G_ERR="✗" ;;
	*) G_OK="-"; G_ERR="x" ;;
esac

die()  { printf '\n%s%s %s%s\n' "$C_ERR" "$G_ERR" "$*" "$C_OFF" >&2; exit 1; }
ok()   { printf '  %s%s%s %s\n' "$C_OK" "$G_OK" "$C_OFF" "$*"; }
note() { printf '  %s%s%s\n' "$C_DIM" "$*" "$C_OFF"; }

command -v git >/dev/null 2>&1 || die "git is required to tell a released version"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)" || die "cannot locate the repository"
cd "$ROOT" || die "cannot enter $ROOT"

BINARY="dist/lookout"
[ -x "$BINARY" ] || die "$BINARY is missing - run tools/build-release.sh first"

VERSION_LINE=$("$BINARY" --version 2>&1) || die "$BINARY does not run here: $VERSION_LINE"
VERSION="${VERSION_LINE#Lookout }"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "'$VERSION_LINE' does not name a version"

# A published release never changes, so its version is packaged only from a clean checkout of its tag.
if git rev-parse -q --verify "refs/tags/v$VERSION" >/dev/null \
	&& { [ "$(git rev-parse HEAD)" != "$(git rev-parse "v$VERSION^{commit}")" ] || [ -n "$(git status --porcelain)" ]; }; then
	die "Lookout $VERSION is released - package it only from a clean checkout of v$VERSION, or rebuild with a newer version"
fi

NAME="lookout-$VERSION-x86_64"
OUT="dist/$NAME.tar.xz"

printf '\n%sPackaging Lookout %s%s\n' "$C_B" "$VERSION" "$C_OFF"
note "$BINARY, built $(date -r "$BINARY" '+%Y-%m-%d %H:%M')"

WORK=$(mktemp -d) || die "cannot create a temporary directory"
trap 'rm -rf "$WORK"' EXIT
STAGE="$WORK/$NAME"

mkdir -m 755 "$STAGE" || die "cannot create $STAGE"
install -m 755 "$BINARY" tools/package/install.sh tools/package/uninstall.sh "$STAGE/" || die "cannot stage the programs"
install -m 644 data/lookout.desktop data/lookout.svg LICENSE tools/package/README.txt "$STAGE/" || die "cannot stage the files"

tar -C "$WORK" --owner=0 --group=0 --numeric-owner --sort=name -cJf "$OUT" "$NAME" || die "cannot write $OUT"

ok "$OUT  $(( $(stat -c%s "$OUT") / 1024 )) KiB"
