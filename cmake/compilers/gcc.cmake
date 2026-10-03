add_library(LktCompileFlags INTERFACE)
target_compile_options(LktCompileFlags INTERFACE
	-Wall
	-Wextra
	-Werror
	-Wno-unused-parameter
	$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
)

message(STATUS "GCC ${CMAKE_CXX_COMPILER_VERSION} compiler flags configured")
