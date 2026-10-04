#!/bin/bash
# Check a package in clean containers of the distros Lookout supports: it installs, lists servers and
# uninstalls cleanly everywhere, and on two of them its window starts under Xvfb.
#
#   tools/check-package.sh [dist/lookout-<version>-x86_64.tar.xz]
#
# Needs the network: the images, their package mirrors, and live Kingpin and UT2004 masters.

set -u

LIST_IMAGES="ubuntu:22.04 ubuntu:24.04 debian:12 debian:13 fedora:latest archlinux:latest"
GUI_IMAGES="ubuntu:22.04 fedora:latest"

# Xvfb, Mesa and the X libraries SDL loads by soname; a desktop has them, a container does not.
APT_GUI="apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
xvfb libgl1 libgl1-mesa-dri libegl1 libx11-6 libxext6 libxcursor1 libxi6 libxfixes3 libxrandr2 libxrender1 libxkbcommon0"
DNF_GUI="dnf -y install xorg-x11-server-Xvfb mesa-dri-drivers mesa-libGL mesa-libEGL \
libX11 libXext libXcursor libXi libXfixes libXrandr libXrender libxkbcommon"

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
bad()  { printf '  %s%s%s %s\n' "$C_ERR" "$G_ERR" "$C_OFF" "$*"; }
note() { printf '  %s%s%s\n' "$C_DIM" "$*" "$C_OFF"; }

command -v docker >/dev/null 2>&1 || die "docker is required to check the package"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)" || die "cannot locate the repository"
cd "$ROOT" || die "cannot enter $ROOT"

TARBALL="${1:-$(ls -t dist/lookout-*-x86_64.tar.xz 2>/dev/null | head -n 1)}"
[ -n "$TARBALL" ] && [ -f "$TARBALL" ] || die "no package to check - run tools/make-package.sh first"

WORK=$(mktemp -d) || die "cannot create a temporary directory"
trap 'rm -rf "$WORK"' EXIT

# Unpacked here: a slim image may have no xz.
mkdir "$WORK/package" "$WORK/checks" || die "cannot prepare $WORK"
tar -C "$WORK/package" -xJf "$TARBALL" || die "cannot unpack $TARBALL"
PACKAGE=$(find "$WORK/package" -mindepth 1 -maxdepth 1 -type d | head -n 1)
[ -n "$PACKAGE" ] && [ -f "$PACKAGE/install.sh" ] || die "$TARBALL holds no install.sh"

cat > "$WORK/checks/list.sh" <<'EOF'
set -u
fail() { echo "FAIL $1"; [ -f /tmp/out ] && tail -n 5 /tmp/out; exit 1; }
cp -R /pkg /tmp/pkg || fail "copying the package"
sh /tmp/pkg/install.sh > /tmp/out 2>&1 || fail "install.sh"
sh /tmp/pkg/install.sh > /tmp/out 2>&1 || fail "install.sh over an existing install"
# One game on a UDP master and one on a TCP master, each through its own protocol script.
listed=""
for game in kingpin ut2004; do
	"$HOME/.local/bin/lookout" --list "$game" > /tmp/list 2> /tmp/out || fail "lookout --list $game"
	[ -s /tmp/list ] || fail "lookout --list $game printed nothing"
	listed="$listed${listed:+ and }$(wc -l < /tmp/list) $game"
done
for uninstaller in "$HOME/.local/share/lookout/uninstall.sh" /tmp/pkg/uninstall.sh; do
	[ -f "$HOME/.local/bin/lookout" ] || sh /tmp/pkg/install.sh > /tmp/out 2>&1 || fail "install.sh, again"
	sh "$uninstaller" > /tmp/out 2>&1 || fail "$uninstaller"
	grep -q "is uninstalled" /tmp/out || fail "$uninstaller found nothing to remove"
	for leftover in "$HOME/.local/bin/lookout" "$HOME/.local/share/applications/lookout.desktop" \
		"$HOME/.local/share/icons/hicolor/scalable/apps/lookout.svg" "$HOME/.local/share/lookout"; do
		[ ! -e "$leftover" ] || fail "$uninstaller left $leftover"
	done
done
echo "PASS installs, lists $listed servers, uninstalls cleanly with either uninstaller"
EOF

cat > "$WORK/checks/gui.sh" <<'EOF'
set -u
fail() { echo "FAIL $1"; [ -f /tmp/out ] && tail -n 5 /tmp/out; exit 1; }
sh -c "$SETUP" > /tmp/out 2>&1 || fail "installing Xvfb and the X and GL libraries"
cp -R /pkg /tmp/pkg || fail "copying the package"
sh /tmp/pkg/install.sh > /tmp/out 2>&1 || fail "install.sh"
Xvfb :99 -screen 0 1280x800x24 > /tmp/xvfb.out 2>&1 &
waited=0
while [ ! -S /tmp/.X11-unix/X99 ] && [ "$waited" -lt 40 ]; do
	sleep 0.25
	waited=$((waited + 1))
done
[ -S /tmp/.X11-unix/X99 ] || { cp /tmp/xvfb.out /tmp/out; fail "Xvfb did not start within 10 s"; }
status=0
DISPLAY=:99 timeout -k 5 5 "$HOME/.local/bin/lookout" > /tmp/out 2>&1 || status=$?
[ "$status" -eq 124 ] || fail "lookout ended with status $status instead of running until the 5 s timeout"
log=$(ls -t "$HOME"/.local/state/lookout/logs/tge_*.log 2>/dev/null | head -n 1)
[ -n "$log" ] || fail "lookout wrote no log"
cp "$log" /tmp/out
display=$(grep -o "video driver 'x11', renderer '[^']*'" "$log") || fail "the log names no x11 video driver and renderer"
echo "PASS window starts under Xvfb: $display"
EOF

# Prints the check's one-line outcome and returns whether it passed.
run_check() {
	local image=$1 script=$2 setup=${3:-} output result
	output=$(docker run --rm -v "$PACKAGE:/pkg:ro" -v "$WORK/checks:/checks:ro" -e SETUP="$setup" "$image" sh "/checks/$script" 2>&1)
	result=$(printf '%s\n' "$output" | grep -E '^(PASS|FAIL) ' | tail -n 1)

	if [[ "$result" == PASS* ]]; then
		ok "$image  ${result#PASS }"
	else
		bad "$image  ${result#FAIL }"
		printf '%s\n' "$output" | sed -n '/^FAIL /,$p' | tail -n +2 | sed 's/^/      /'
		[ -n "$result" ] || printf '%s\n' "$output" | tail -n 5 | sed 's/^/      /'
		false
	fi
}

printf '\n%sChecking %s%s\n' "$C_B" "$TARBALL" "$C_OFF"
note "$LIST_IMAGES; window on $GUI_IMAGES"

failures=0

for image in $LIST_IMAGES; do
	run_check "$image" list.sh || failures=$((failures + 1))
done

for image in $GUI_IMAGES; do
	case "$image" in
		ubuntu:*|debian:*) setup="$APT_GUI" ;;
		*) setup="$DNF_GUI" ;;
	esac

	run_check "$image" gui.sh "$setup" || failures=$((failures + 1))
done

[ "$failures" -eq 0 ] || die "$failures check(s) failed"
ok "every check passed"
