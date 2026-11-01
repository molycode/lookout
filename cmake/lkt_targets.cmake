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

# LktAddWindowsManifest — on Windows, the manifest every Lookout program carries, which makes UTF-8 its code page.
function(LktAddWindowsManifest name)
	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		target_sources(${name} PRIVATE ${PROJECT_SOURCE_DIR}/data/windows/lookout.manifest)
	endif()
endfunction()

# LktAddWindowsResources — on Windows, the manifest with the icon and version Explorer shows. rc.exe reports no
# dependency on the icon, so OBJECT_DEPENDS names it.
function(LktAddWindowsResources name description fileName)
	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		set(LKT_ICON ${PROJECT_SOURCE_DIR}/data/windows/lookout.ico)
		set(LKT_FILE_DESCRIPTION ${description})
		set(LKT_ORIGINAL_FILENAME ${fileName})
		set(resources ${CMAKE_CURRENT_BINARY_DIR}/${name}.rc)

		configure_file(${PROJECT_SOURCE_DIR}/data/windows/lookout.rc.in ${resources} @ONLY)
		set_source_files_properties(${resources} PROPERTIES OBJECT_DEPENDS ${LKT_ICON})
		target_sources(${name} PRIVATE ${resources})
		LktAddWindowsManifest(${name})
	endif()
endfunction()
