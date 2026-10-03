set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Cache variable first, environment as a fallback: a preset's `environment` block never reaches an IDE's
# configure step, so an env-only setting is silently lost there. See the note in clang.cmake.
if(NOT TGE_GCC_PATH AND DEFINED ENV{TGE_GCC_PATH})
	set(TGE_GCC_PATH "$ENV{TGE_GCC_PATH}" CACHE PATH "GCC installation root")
endif()

# try_compile re-runs this file in a scratch project that inherits the environment but NOT the cache, so the
# compiler-ABI probe would otherwise fall back to the system GCC and measure the wrong toolchain.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES TGE_GCC_PATH)

if(NOT DEFINED CMAKE_C_COMPILER)
	if(TGE_GCC_PATH)
		set(CMAKE_C_COMPILER "${TGE_GCC_PATH}/bin/gcc")
	else()
		set(CMAKE_C_COMPILER "gcc")
	endif()
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER)
	if(TGE_GCC_PATH)
		set(CMAKE_CXX_COMPILER "${TGE_GCC_PATH}/bin/g++")
	else()
		set(CMAKE_CXX_COMPILER "g++")
	endif()
endif()

if(TGE_GCC_PATH)
	# Bake in the RPATH so the binary finds the custom libstdc++ at runtime
	set(CMAKE_BUILD_RPATH "${TGE_GCC_PATH}/lib64")
	set(CMAKE_INSTALL_RPATH "${TGE_GCC_PATH}/lib64")
	message(STATUS "Using GCC from TGE_GCC_PATH: ${TGE_GCC_PATH}")
else()
	# Quieter than the Clang toolchain warns: a GCC build brings its own libstdc++, so only the RPATH is lost.
	message(STATUS "Using system GCC (set TGE_GCC_PATH to use custom GCC)")
endif()

message(STATUS "CMAKE_C_COMPILER = ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER = ${CMAKE_CXX_COMPILER}")
