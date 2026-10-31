# As with MSVC: Lookout builds without exceptions, so CMake's default /EHsc goes for every target.
foreach(lktFlags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
	string(REPLACE "/EHsc" "" ${lktFlags} "${${lktFlags}}")
endforeach()

add_library(LktCompileFlags INTERFACE)
target_compile_options(LktCompileFlags INTERFACE
	/W4
	/WX
	/permissive-
	-Wno-unused-parameter
	$<$<COMPILE_LANGUAGE:CXX>:/EHs-c->
)

message(STATUS "clang-cl ${CMAKE_CXX_COMPILER_VERSION} compiler flags configured")
