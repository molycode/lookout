add_subdirectory(query)
add_subdirectory(script)
add_subdirectory(geo)
add_subdirectory(json)
add_subdirectory(games)
add_subdirectory(launch)
add_subdirectory(net)
add_subdirectory(download)
add_subdirectory(browser)
add_subdirectory(config)
add_subdirectory(ui)
add_subdirectory(cli)

file(GLOB LKT_APP_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp")

LktApp(lookout ${LKT_APP_SOURCES})
LktAddWindowsResources(lookout "Lookout" lookout.exe)

target_link_libraries(lookout PRIVATE
	Lkt::Browser
	Lkt::Cli
	Lkt::Games
	Lkt::Launch
	Lkt::Ui
)

set_target_properties(lookout PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	# The window opens no console; a sanitizer build keeps one, as it reports on stderr. The entry point stays main.
	if(LKT_SANITIZER STREQUAL "none")
		set_target_properties(lookout PROPERTIES WIN32_EXECUTABLE ON)
		target_link_options(lookout PRIVATE /ENTRY:mainCRTStartup)
	endif()

	# The command line, which cmd and PowerShell run before lookout.exe for "lookout". Its program database is its own,
	# and an incremental link would share lookout.ilk with the window.
	LktApp(lookout_console console/main.cpp)
	LktAddWindowsResources(lookout_console "Lookout command line" lookout.com)

	target_link_libraries(lookout_console PRIVATE
		Lkt::Cli
		Tge::Process
	)

	set_target_properties(lookout_console PROPERTIES
		OUTPUT_NAME lookout
		SUFFIX .com
		PDB_NAME lookout_console
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
	)

	target_link_options(lookout_console PRIVATE /INCREMENTAL:NO)

	# It starts the window from its own folder.
	add_dependencies(lookout_console lookout)
endif()
