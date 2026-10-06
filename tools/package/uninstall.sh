#!/bin/sh
# Removes what install.sh put in place and the game icons Lookout keeps; settings, logs and the user's game descriptions
# are kept.
#
#   ./uninstall.sh      (from the package, or as ~/.local/share/lookout/uninstall.sh)

set -eu

die() { printf 'uninstall.sh: %s\n' "$*" >&2; exit 1; }

# The XDG base directory spec ignores a relative value.
xdg_dir() {
	case "$1" in
		/*) printf '%s' "$1" ;;
		*) printf '%s' "$HOME/$2" ;;
	esac
}

[ -n "${HOME:-}" ] || die "HOME is not set"

DATA_DIR=$(xdg_dir "${XDG_DATA_HOME:-}" .local/share)
CONFIG_DIR=$(xdg_dir "${XDG_CONFIG_HOME:-}" .config)/lookout
STATE_DIR=$(xdg_dir "${XDG_STATE_HOME:-}" .local/state)/lookout
CACHE_DIR=$(xdg_dir "${XDG_CACHE_HOME:-}" .cache)/lookout
APPS_DIR="$DATA_DIR/applications"
UNINSTALL_DIR="$DATA_DIR/lookout"
isFound=false

for file in "$HOME/.local/bin/lookout" "$APPS_DIR/lookout.desktop" "$DATA_DIR/icons/hicolor/scalable/apps/lookout.svg" \
	"$UNINSTALL_DIR/uninstall.sh"; do
	if [ -e "$file" ]; then
		rm -f "$file"
		isFound=true
	fi
done

rm -rf "$CACHE_DIR"
rmdir "$UNINSTALL_DIR" 2>/dev/null || true

# Still there, it holds the downloaded games or the user's own.
if [ "$isFound" = true ] && [ -d "$UNINSTALL_DIR" ]; then
	echo "Lookout is uninstalled. Its settings in $CONFIG_DIR, logs in $STATE_DIR and game descriptions in $UNINSTALL_DIR are kept."
elif [ "$isFound" = true ]; then
	echo "Lookout is uninstalled. Its settings in $CONFIG_DIR and logs in $STATE_DIR are kept."
else
	echo "Lookout is not installed for this user: there was nothing to remove."
fi
