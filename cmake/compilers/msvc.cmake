# CMake's default /EHsc would turn exceptions back on for every target; Lookout builds without them, as -fno-exceptions
# does, and C4530 is only the STL saying so.
foreach(lktFlags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
	string(REPLACE "/EHsc" "" ${lktFlags} "${${lktFlags}}")
endforeach()

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/wd4530>)

# Global, so vendored sources such as tge-core's are read as UTF-8 too, not in the machine's code page.
add_compile_options(/utf-8)

add_library(LktCompileFlags INTERFACE)
target_compile_options(LktCompileFlags INTERFACE
	/W4
	/WX
	/permissive-
	/wd4100
	$<$<COMPILE_LANGUAGE:CXX>:/EHs-c->
)

message(STATUS "MSVC ${CMAKE_CXX_COMPILER_VERSION} compiler flags configured")
