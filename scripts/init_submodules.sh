#!/bin/bash
#
# Populates this application's submodules to exactly the depth it builds.
#
# `git submodule update --init --recursive` is the command NOT to run here: it also enters whatever the vendored
# projects carry for their own test suites and tooling.
#
# Run from anywhere; paths resolve against the repository root.
set -eu

readonly ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Mbed TLS's git tags lack the sources its release scripts generate, so the release tarball is the source instead.
readonly MBEDTLS_VERSION="4.1.1"
readonly MBEDTLS_SHA256="3359a349e23db3d5536fcee032ae7b2ecbfc08972fab643089b5cbf2a375c98c"
readonly MBEDTLS_URL="https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-${MBEDTLS_VERSION}/mbedtls-${MBEDTLS_VERSION}.tar.bz2"

cd "${ROOT}"

echo "== tge-core, SDL, Dear ImGui, nlohmann/json, Lua, googletest, lookout-games"
git submodule update --init --filter=blob:none

# Core compiles GLM and rpmalloc into its own targets, so both are needed and neither declares submodules of
# its own -- recursion has nothing further to descend into here.
echo "== Core's own dependencies (glm, rpmalloc)"
git -C external/tge-core submodule update --init --recursive --filter=blob:none

if [ ! -f external/mbedtls/CMakeLists.txt ]; then
	echo "== Mbed TLS ${MBEDTLS_VERSION}"
	download="$(mktemp -d)"
	trap 'rm -rf "${download}"' EXIT
	curl --fail --silent --show-error --location --output "${download}/mbedtls.tar.bz2" "${MBEDTLS_URL}"
	echo "${MBEDTLS_SHA256}  ${download}/mbedtls.tar.bz2" | sha256sum --check --quiet
	rm -rf external/mbedtls
	mkdir -p external/mbedtls
	tar -xjf "${download}/mbedtls.tar.bz2" -C external/mbedtls --strip-components=1
fi

echo
echo "Done. Checked out $(du -sh --exclude=.git external | cut -f1) of sources, $(du -sh .git/modules | cut -f1) of git."
echo "Build with:  cmake --preset <preset> && cmake --build --preset <preset>"
