cmake_minimum_required(VERSION 4.3.0)

project(Lookout VERSION 1.7.0 LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Windows)$")
	message(FATAL_ERROR "Lookout runs on Linux and Windows; '${CMAKE_SYSTEM_NAME}' is not supported")
endif()

if(PROJECT_IS_TOP_LEVEL)
	set(CMAKE_CXX_STANDARD 23)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	set(CMAKE_CXX_EXTENSIONS OFF)
	set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/compile_flags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/lkt_targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/sanitizer.cmake)

option(LKT_STATIC_RUNTIME "Link the C and C++ runtime statically" OFF)

if(LKT_STATIC_RUNTIME AND MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(LKT_STATIC_RUNTIME)
	add_link_options(-static-libstdc++ -static-libgcc)
endif()

option(LKT_BUILD_TESTS "Build Lookout's test suite" ${PROJECT_IS_TOP_LEVEL})

option(LKT_AUTO_INIT_SUBMODULES "Populate missing submodule checkouts during configure" ON)

# A fresh clone carries the submodule mount points with nothing inside them, and the block below reads an
# absent external/tge-core/CMakeLists.txt as "resolve from packages" -- so the first find_package would report
# a missing TgeCore and name the wrong remedy.
if(PROJECT_IS_TOP_LEVEL
		AND ((IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external/tge-core
				AND NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/tge-core/CMakeLists.txt)
			OR NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/json/single_include/nlohmann/json.hpp
			OR NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/lua/lua.h
			OR NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/mbedtls/CMakeLists.txt
			OR (LKT_BUILD_TESTS AND NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/googletest/CMakeLists.txt)
			OR (LKT_BUILD_TESTS AND NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/lookout-games/index.json)))
	if(NOT LKT_AUTO_INIT_SUBMODULES)
		message(FATAL_ERROR
			"Lookout: external/ holds empty submodule mount points. Run cmake -P scripts/init_submodules.cmake, or "
			"configure with -DLKT_AUTO_INIT_SUBMODULES=ON to have this do it.")
	endif()

	message(STATUS "[Lookout] submodules not initialized -- running scripts/init_submodules.cmake")

	execute_process(
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/init_submodules.cmake
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		RESULT_VARIABLE lktInitResult
	)

	if(NOT lktInitResult EQUAL 0)
		message(FATAL_ERROR
			"Lookout: scripts/init_submodules.cmake failed (${lktInitResult}). Run it by hand to see "
			"why -- a network or credential problem is the usual cause.")
	endif()
endif()

if(LKT_BUILD_TESTS AND NOT TARGET GTest::gtest)
	add_subdirectory(external/googletest ${CMAKE_BINARY_DIR}/tests/googletest EXCLUDE_FROM_ALL SYSTEM)
	set(lktAddedGoogletest ON)
endif()

if(LKT_BUILD_TESTS)
	# A normal variable, because a cache entry would lose to the OFF an earlier configure already cached.
	set(TGE_CORE_TESTING ON)
	set(lktTgeComponents COMPONENTS Testing)
endif()

if(NOT TARGET Tge::Core AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/tge-core/CMakeLists.txt)
	add_subdirectory(external/tge-core EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET Tge::Core)
	find_package(TgeCore 1.0.0 REQUIRED ${lktTgeComponents})
endif()

# Silenced only now: tge-core's helpers arrive with it, and googletest has to come first for its Testing component.
if(lktAddedGoogletest)
	tge_suppress_external_warnings_in_directory(external/googletest)
endif()

# Static, with every platform library loaded by soname at run time, so one binary serves X11 and Wayland anywhere.
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

foreach(lktUnusedSubsystem AUDIO CAMERA GPU JOYSTICK HAPTIC HIDAPI POWER SENSOR TRAY)
	set(SDL_${lktUnusedSubsystem} OFF CACHE BOOL "" FORCE)
endforeach()

foreach(lktUnusedFeature X11_XSCRNSAVER X11_XTEST FRIBIDI LIBTHAI IBUS LIBUDEV)
	set(SDL_${lktUnusedFeature} OFF CACHE BOOL "" FORCE)
endforeach()

add_subdirectory(external/sdl EXCLUDE_FROM_ALL SYSTEM)
tge_suppress_external_warnings_in_directory(external/sdl)

set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(TF_PSA_CRYPTO_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON CACHE BOOL "" FORCE)
set(MBEDTLS_USER_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mbedtls/lookout_tls_config.h CACHE FILEPATH "" FORCE)
set(TF_PSA_CRYPTO_USER_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mbedtls/lookout_crypto_config.h CACHE FILEPATH "" FORCE)

# Mbed TLS includes threading_alt.h from its own private targets as well as from Lookout's files, so only a
# directory-wide path reaches them all.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	include_directories(cmake/mbedtls/windows)
endif()

add_subdirectory(external/mbedtls EXCLUDE_FROM_ALL SYSTEM)

foreach(lktMbedtlsTarget mbedtls mbedx509 tfpsacrypto builtin everest extras p256m)
	tge_suppress_external_warnings(${lktMbedtlsTarget})
endforeach()

add_subdirectory(src)

if(LKT_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
