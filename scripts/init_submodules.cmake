# Populates this application's submodules to exactly the depth it builds, and fetches Mbed TLS.
#
#   cmake -P scripts/init_submodules.cmake
#
# `git submodule update --init --recursive` is the command NOT to run here: it also enters whatever the vendored
# projects carry for their own test suites and tooling.
cmake_minimum_required(VERSION 4.3.0)

get_filename_component(lktRoot "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Mbed TLS's git tags lack the sources its release scripts generate, so the release tarball is the source instead.
set(lktMbedtlsVersion "4.1.1")
set(lktMbedtlsSha256 "3359a349e23db3d5536fcee032ae7b2ecbfc08972fab643089b5cbf2a375c98c")
set(lktMbedtlsUrl "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-${lktMbedtlsVersion}/mbedtls-${lktMbedtlsVersion}.tar.bz2")

find_package(Git REQUIRED)

function(LktRunGit)
	execute_process(COMMAND ${GIT_EXECUTABLE} ${ARGN} WORKING_DIRECTORY ${lktRoot} RESULT_VARIABLE result)

	if(NOT result EQUAL 0)
		message(FATAL_ERROR "git ${ARGN} failed (${result})")
	endif()
endfunction()

# Fixtures, embedded licences and lookout-games' hashed files must keep their bytes on a Windows checkout too.
message(STATUS "== tge-core, SDL, Dear ImGui, nlohmann/json, Lua, googletest, lookout-games")
LktRunGit(-c core.autocrlf=false submodule update --init --filter=blob:none)

# Core compiles GLM and rpmalloc into its own targets, so both are needed and neither declares submodules of
# its own -- recursion has nothing further to descend into here.
message(STATUS "== Core's own dependencies (glm, rpmalloc)")
LktRunGit(-C external/tge-core -c core.autocrlf=false submodule update --init --recursive --filter=blob:none)
LktRunGit(submodule foreach --quiet --recursive "git config core.autocrlf false")

if(NOT EXISTS "${lktRoot}/external/mbedtls/CMakeLists.txt")
	message(STATUS "== Mbed TLS ${lktMbedtlsVersion}")
	set(download "${lktRoot}/build/mbedtls-download")
	file(REMOVE_RECURSE "${download}")
	file(DOWNLOAD "${lktMbedtlsUrl}" "${download}/mbedtls.tar.bz2" EXPECTED_HASH SHA256=${lktMbedtlsSha256} STATUS status)
	list(GET status 0 statusCode)

	if(NOT statusCode EQUAL 0)
		message(FATAL_ERROR "Downloading ${lktMbedtlsUrl} failed: ${status}")
	endif()

	file(ARCHIVE_EXTRACT INPUT "${download}/mbedtls.tar.bz2" DESTINATION "${download}/extracted")
	file(REMOVE_RECURSE "${lktRoot}/external/mbedtls")
	file(RENAME "${download}/extracted/mbedtls-${lktMbedtlsVersion}" "${lktRoot}/external/mbedtls")
	file(REMOVE_RECURSE "${download}")
endif()

message(STATUS "Done. Build with:  cmake --preset <preset> && cmake --build --preset <preset>")
