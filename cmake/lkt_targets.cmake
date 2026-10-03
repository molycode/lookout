# Read as a property, not linked: a PRIVATE link on a static library still lands in INTERFACE_LINK_LIBRARIES
# as $<LINK_ONLY:LktCompileFlags>, dragging a build-only target into every export set.
function(LktApplyCompileFlags name)
	target_compile_options(${name} PRIVATE $<TARGET_PROPERTY:LktCompileFlags,INTERFACE_COMPILE_OPTIONS>)
endfunction()

# LktLib — a Lookout-owned library. STATIC/OBJECT targets get LktCompileFlags; INTERFACE do not.
# The namespaced alias is what consumers link; an unknown Lkt::Foo fails at generate time, not at link.
function(LktLib name type)
	add_library(${name} ${type} ${ARGN})
	string(REGEX REPLACE "^Lkt" "" aliasName "${name}")
	add_library(Lkt::${aliasName} ALIAS ${name})

	if(NOT "${type}" STREQUAL "INTERFACE")
		LktApplyCompileFlags(${name})
	endif()
endfunction()

# LktApp — a Lookout-owned executable.
function(LktApp name)
	add_executable(${name} ${ARGN})
	LktApplyCompileFlags(${name})
endfunction()
