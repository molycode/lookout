# Selects the compiler flag set for Lookout's own code, creating the LktCompileFlags INTERFACE library.

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	include(${CMAKE_CURRENT_LIST_DIR}/compilers/msvc.cmake)
elseif(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
	include(${CMAKE_CURRENT_LIST_DIR}/compilers/clang-cl.cmake)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	include(${CMAKE_CURRENT_LIST_DIR}/compilers/gcc.cmake)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "[Cc]lang")
	include(${CMAKE_CURRENT_LIST_DIR}/compilers/clang.cmake)
else()
	message(FATAL_ERROR "Lookout builds with GCC, Clang, MSVC or clang-cl; '${CMAKE_CXX_COMPILER_ID}' is not supported")
endif()
