#!/bin/bash
# Build the release binary dist/lookout, the one that ships.
#
#   LKT_CMAKE_DIR=<cmake> LKT_NINJA_DIR=<ninja> tools/build-release.sh
#
# Built in AlmaLinux 9 because nothing at run time can detect a glibc floor that is too high; the gates refuse it here.

set -u

IMAGE="almalinux:9@sha256:3a3fa7f043b142bc8008c8b308d39b47d2c84008addcd52f9f9a7a82d2a90474"
TOOLSET="gcc-toolset-15"
MAX_GLIBC="2.35"
CMAKE_DIR="${LKT_CMAKE_DIR:-}"
NINJA_DIR="${LKT_NINJA_DIR:-}"

# Every library the binary may name as NEEDED: glibc's own. SDL loads everything else by soname at run time.
ALLOWED_NEEDED="libc.so.6 libm.so.6 ld-linux-x86-64.so.2 libdl.so.2 libpthread.so.0"

# What SDL silently drops when a development package is missing, and what the binary cannot do without.
REQUIRED_SDL_FEATURES="SDL_VIDEO_DRIVER_WAYLAND SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_LIBDECOR SDL_VIDEO_DRIVER_X11 SDL_VIDEO_RENDER_OGL HAVE_DBUS_DBUS_H"

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

command -v docker >/dev/null 2>&1 || die "docker is required to build the release"
command -v readelf >/dev/null 2>&1 || die "readelf is required to check the result"
command -v objdump >/dev/null 2>&1 || die "objdump is required to check the result"
[ -n "$CMAKE_DIR" ] && [ -x "$CMAKE_DIR/bin/cmake" ] || die "no cmake in '$CMAKE_DIR' - set LKT_CMAKE_DIR to an unpacked CMake 4.3+ release"
[ -n "$NINJA_DIR" ] && [ -x "$NINJA_DIR/ninja" ] || die "no ninja in '$NINJA_DIR' - set LKT_NINJA_DIR to the folder holding ninja"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)" || die "cannot locate the repository"
cd "$ROOT" || die "cannot enter $ROOT"

for submodule in external/tge-core external/sdl external/imgui external/json external/lua external/googletest; do
	[ -f "$submodule/CMakeLists.txt" ] || [ -f "$submodule/imgui.h" ] || [ -f "$submodule/lua.h" ] \
		|| die "$submodule is not populated - run scripts/init_submodules.sh"
done

OUT_DIR="dist"
WORK=$(mktemp -d) || die "cannot create a temporary directory"
trap 'rm -rf "$WORK"' EXIT

printf '\n%sBuilding the release%s\n' "$C_B" "$C_OFF"
note "$IMAGE, $TOOLSET"

LOG_OUT="/dev/null"
[ -n "${LKT_VERBOSE:-}" ] && LOG_OUT="/dev/stderr"

# Read-only mounts and the result on stdout as a tar: a root container must not leave root-owned files.
docker run --rm -i \
	-v "$ROOT:/src:ro" \
	-v "$CMAKE_DIR:/opt/cmake:ro" \
	-v "$NINJA_DIR:/opt/ninja:ro" \
	-e LOG_OUT="$LOG_OUT" \
	-e TOOLSET="$TOOLSET" \
	"$IMAGE" \
	/bin/bash -c '
		set -e
		{
			dnf -y install dnf-plugins-core
			dnf config-manager --set-enabled crb
			dnf -y install "$TOOLSET-gcc-c++" "$TOOLSET-libstdc++-devel" libstdc++-static \
				libX11-devel libXext-devel libXcursor-devel libXi-devel libXfixes-devel libXrandr-devel libXrender-devel \
				libxkbcommon-devel wayland-devel wayland-protocols-devel libdecor-devel \
				mesa-libEGL-devel mesa-libGL-devel dbus-devel
		} > "$LOG_OUT" 2>&1
		source "/opt/rh/$TOOLSET/enable"
		export PATH="/opt/cmake/bin:/opt/ninja:$PATH"
		cmake -S /src -B /build -G Ninja \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchains/linux/gcc.cmake \
			-DLKT_STATIC_RUNTIME=ON \
			-DLKT_AUTO_INIT_SUBMODULES=OFF > "$LOG_OUT" 2>&1
		cmake --build /build >&2
		ctest --test-dir /build --output-on-failure >&2
		strip /build/lookout
		config=$(find /build/external/sdl -name SDL_build_config.h | head -1)
		cp "$config" /build/SDL_build_config.h
		tar -C /build -cf - lookout SDL_build_config.h
	' > "$WORK/release.tar" || die "the container build or its tests failed - re-run with LKT_VERBOSE=1"

tar -C "$WORK" -xf "$WORK/release.tar" || die "the container produced no usable result"
[ -s "$WORK/lookout" ] || die "the container produced no binary"

for feature in $REQUIRED_SDL_FEATURES; do
	grep -qE "^#define $feature( |$)" "$WORK/SDL_build_config.h" \
		|| die "SDL was built without $feature - a development package is missing from the container"
done
ok "SDL features: $REQUIRED_SDL_FEATURES"

FLOOR=$(readelf -V "$WORK/lookout" 2>/dev/null | grep -oE 'GLIBC_[0-9.]+' | sort -uV | tail -1)
[ -n "$FLOOR" ] || die "cannot read the glibc version needs of the build"
FLOOR_SYMBOLS=$(objdump -T "$WORK/lookout" | grep -F "($FLOOR)" | awk '{print $NF}' | sort -u | tr '\n' ' ')
[ "$(printf '%s\n%s\n' "${FLOOR#GLIBC_}" "$MAX_GLIBC" | sort -V | tail -1)" = "$MAX_GLIBC" ] \
	|| die "the build needs $FLOOR (for $FLOOR_SYMBOLS), newer than GLIBC_$MAX_GLIBC - the build host was too new"
ok "glibc floor $FLOOR (at most GLIBC_$MAX_GLIBC), set by: $FLOOR_SYMBOLS"

for needed in $(readelf -d "$WORK/lookout" | grep NEEDED | sed -E 's/.*\[(.*)\]/\1/'); do
	case " $ALLOWED_NEEDED " in
		*" $needed "*) ;;
		*) die "the binary needs $needed, which is not on the allowlist" ;;
	esac
done
ok "needs only: $(readelf -d "$WORK/lookout" | grep NEEDED | sed -E 's/.*\[(.*)\]/\1/' | tr '\n' ' ')"

mkdir -p "$OUT_DIR" || die "cannot create $OUT_DIR"
mv "$WORK/lookout" "$OUT_DIR/lookout" || die "cannot write $OUT_DIR/lookout"
chmod 755 "$OUT_DIR/lookout"

ok "$OUT_DIR/lookout  $(( $(stat -c%s "$OUT_DIR/lookout") / 1024 )) KiB"
