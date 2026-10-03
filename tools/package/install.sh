#!/bin/sh
# Installs Lookout for the current user under ~/.local, with a menu entry, an icon and an uninstaller.
#
#   ./install.sh        (from the folder the package unpacked to)

set -eu

die() { printf 'install.sh: %s\n' "$*" >&2; exit 1; }

# The XDG base directory spec ignores a relative value.
xdg_dir() {
	case "$1" in
		/*) printf '%s' "$1" ;;
		*) printf '%s' "$HOME/$2" ;;
	esac
}

[ -n "${HOME:-}" ] || die "HOME is not set"

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BIN_DIR="$HOME/.local/bin"
DATA_DIR=$(xdg_dir "${XDG_DATA_HOME:-}" .local/share)
CONFIG_DIR=$(xdg_dir "${XDG_CONFIG_HOME:-}" .config)/lookout
STATE_DIR=$(xdg_dir "${XDG_STATE_HOME:-}" .local/state)/lookout
APPS_DIR="$DATA_DIR/applications"
ICONS_DIR="$DATA_DIR/icons/hicolor"
BINARY="$BIN_DIR/lookout"
DESKTOP="$APPS_DIR/lookout.desktop"
ICON="$ICONS_DIR/scalable/apps/lookout.svg"
UNINSTALL="$DATA_DIR/lookout/uninstall.sh"

for file in lookout lookout.desktop lookout.svg uninstall.sh; do
	[ -f "$HERE/$file" ] || die "$file is missing beside install.sh - unpack the whole package"
done

# These need escaping in a desktop entry's Exec.
case "$BINARY" in
	*'"'* | *'`'* | *'$'* | *'\'* | *'%'*)
		die "cannot install under $HOME: the path holds a quote, backquote, dollar, backslash or percent sign" ;;
esac

VERSION=$("$HERE/lookout" --version 2>&1) \
	|| die "lookout does not start here: $VERSION"

mkdir -p "$BIN_DIR" "$APPS_DIR" "$ICONS_DIR/scalable/apps" "$(dirname "$UNINSTALL")"

# First, so an install that fails part-way can still be undone.
cp "$HERE/uninstall.sh" "$UNINSTALL"
chmod 755 "$UNINSTALL"

# Through a rename: copying over a running lookout fails with "Text file busy".
cp "$HERE/lookout" "$BINARY.new"
chmod 755 "$BINARY.new"
mv -f "$BINARY.new" "$BINARY"

while IFS= read -r line; do
	case "$line" in
		Exec=*) printf 'Exec="%s"\n' "$BINARY" ;;
		TryExec=*) printf 'TryExec=%s\n' "$BINARY" ;;
		*) printf '%s\n' "$line" ;;
	esac
done < "$HERE/lookout.desktop" > "$DESKTOP"
chmod 644 "$DESKTOP"

cp "$HERE/lookout.svg" "$ICON"
chmod 644 "$ICON"

# GTK rescans an icon folder whose time changed; there is no cache to update in a user's own theme folder.
touch "$ICONS_DIR" || true

echo "$VERSION is installed:"
echo "  $BINARY"
echo "  $DESKTOP"
echo "  $ICON"

case ":${PATH:-}:" in
	*":$BIN_DIR:"*) echo "Start it from the applications menu, or run: lookout" ;;
	*) echo "Start it from the applications menu, or run: $BINARY" ;;
esac

echo "To uninstall it: $HERE/uninstall.sh (or $UNINSTALL once this folder is gone)"
