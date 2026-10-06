# One option rather than raw flags in a preset: a preset that sets CMAKE_CXX_FLAGS replaces the toolchain's.
set(LKT_SANITIZER "none" CACHE STRING "Sanitizer to build with: none, address, undefined, address,undefined or thread")
set_property(CACHE LKT_SANITIZER PROPERTY STRINGS none address undefined address,undefined thread)

if(NOT LKT_SANITIZER STREQUAL "none")
	if(NOT LKT_SANITIZER MATCHES "^(address|undefined|address,undefined|thread)$")
		message(FATAL_ERROR "LKT_SANITIZER is '${LKT_SANITIZER}'; expected none, address, undefined, address,undefined or thread")
	endif()

	add_compile_options(-fsanitize=${LKT_SANITIZER} -fno-omit-frame-pointer)
	add_link_options(-fsanitize=${LKT_SANITIZER})
	add_compile_definitions(LKT_SANITIZER_ENABLED)

	message(STATUS "Sanitizer enabled: ${LKT_SANITIZER}")
endif()
