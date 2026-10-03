set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Both paths accept a cache variable and fall back to the environment. An environment-only setting is
# unreachable from an IDE: QtCreator translates a preset into -D arguments and applies the preset's
# `environment` block to the BUILD step, never to the configure — so the toolchain would never see it.
if(NOT TGE_CLANG_PATH AND DEFINED ENV{TGE_CLANG_PATH})
	set(TGE_CLANG_PATH "$ENV{TGE_CLANG_PATH}" CACHE PATH "Clang installation root")
endif()

if(NOT TGE_GCC_PATH AND DEFINED ENV{TGE_GCC_PATH})
	set(TGE_GCC_PATH "$ENV{TGE_GCC_PATH}" CACHE PATH "GCC installation supplying libstdc++ to Clang builds")
endif()

# try_compile re-runs this file in a scratch project that inherits the environment but NOT the cache, so
# without this the compiler-ABI probe would measure a different libstdc++ than the build then uses.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES TGE_CLANG_PATH TGE_GCC_PATH)

if(TGE_CLANG_PATH)
	set(CMAKE_C_COMPILER "${TGE_CLANG_PATH}/bin/clang" CACHE FILEPATH "" FORCE)
	set(CMAKE_CXX_COMPILER "${TGE_CLANG_PATH}/bin/clang++" CACHE FILEPATH "" FORCE)
elseif(NOT DEFINED CMAKE_C_COMPILER)
	set(CMAKE_C_COMPILER "clang")
	set(CMAKE_CXX_COMPILER "clang++")
endif()

# Clang has no standard library of its own here, so point it at the intended GCC's libstdc++ and bake the
# RPATH so the binary finds that same one at runtime.
if(TGE_GCC_PATH)
	file(GLOB _gcc_ver_dirs LIST_DIRECTORIES true "${TGE_GCC_PATH}/lib/gcc/x86_64-pc-linux-gnu/*")
	list(SORT _gcc_ver_dirs COMPARE NATURAL ORDER DESCENDING)
	list(GET _gcc_ver_dirs 0 _gcc_install_dir)
	add_compile_options(--gcc-install-dir=${_gcc_install_dir})
	add_link_options(--gcc-install-dir=${_gcc_install_dir})
	set(CMAKE_BUILD_RPATH "${TGE_GCC_PATH}/lib64")
	set(CMAKE_INSTALL_RPATH "${TGE_GCC_PATH}/lib64")
	message(STATUS "Using GCC libstdc++ from TGE_GCC_PATH: ${TGE_GCC_PATH}")
else()
	# Warning, not error: a recent enough system GCC works, and probing for what the engine needs today would
	# want revisiting at every standard bump.
	message(WARNING
		"TGE_GCC_PATH is not set — Clang takes libstdc++ from whichever system GCC it selects. "
		"If that one is too old, the build fails while compiling, not here.")
endif()

if(TGE_CLANG_PATH)
	message(STATUS "Using Clang from TGE_CLANG_PATH: ${TGE_CLANG_PATH}")
else()
	message(STATUS "Using system Clang (set TGE_CLANG_PATH to use custom Clang)")
endif()

message(STATUS "CMAKE_C_COMPILER = ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER = ${CMAKE_CXX_COMPILER}")
