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

file(GLOB LKT_APP_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp")

LktApp(lookout ${LKT_APP_SOURCES})

target_compile_definitions(lookout PRIVATE LKT_VERSION="${PROJECT_VERSION}")

target_link_libraries(lookout PRIVATE
	Lkt::Browser
	Lkt::Config
	Lkt::Games
	Lkt::Launch
	Lkt::Net
	Lkt::Query
	Lkt::Ui
	Tge::Lifecycle
	Tge::Logging
)

set_target_properties(lookout PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
