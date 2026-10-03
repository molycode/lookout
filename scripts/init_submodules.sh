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

cd "${ROOT}"

echo "== tge-core, SDL, Dear ImGui, nlohmann/json, googletest"
git submodule update --init --filter=blob:none

# Core compiles GLM and rpmalloc into its own targets, so both are needed and neither declares submodules of
# its own -- recursion has nothing further to descend into here.
echo "== Core's own dependencies (glm, rpmalloc)"
git -C external/tge-core submodule update --init --recursive --filter=blob:none

echo
echo "Done. Checked out $(du -sh --exclude=.git external | cut -f1) of sources, $(du -sh .git/modules | cut -f1) of git."
echo "Build with:  cmake --preset <preset> && cmake --build --preset <preset>"
