add_library(LktCompileFlags INTERFACE)
target_compile_options(LktCompileFlags INTERFACE
	-Wall
	-Wextra
	-Werror
	-Wno-unused-parameter
	$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
	$<$<COMPILE_LANGUAGE:CXX>:-stdlib=libstdc++>
)

message(STATUS "Clang ${CMAKE_CXX_COMPILER_VERSION} compiler flags configured")
