file(GLOB_RECURSE LKT_TEST_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp")

LktApp(LookoutTests ${LKT_TEST_SOURCES})

target_compile_definitions(LookoutTests PRIVATE LKT_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
	LKT_TEST_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scripts" LKT_TEST_GAMES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/games"
	LKT_LOOKOUT_GAMES_DIR="${PROJECT_SOURCE_DIR}/external/lookout-games")

# Pure logic private to its module by design, such as the network scheduler or the settings reader; reaching in
# beats widening an interface for a test.
target_include_directories(LookoutTests
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		$<TARGET_PROPERTY:LktBrowser,SOURCE_DIR>/private
		$<TARGET_PROPERTY:LktConfig,SOURCE_DIR>/private
		$<TARGET_PROPERTY:LktDownload,SOURCE_DIR>/private
		$<TARGET_PROPERTY:LktGames,SOURCE_DIR>/private
		$<TARGET_PROPERTY:LktGeo,SOURCE_DIR>/private
		$<TARGET_PROPERTY:LktLaunch,SOURCE_DIR>/private
		$<TARGET_PROPERTY:LktNet,SOURCE_DIR>/private
		$<TARGET_PROPERTY:LktUi,SOURCE_DIR>/private
)

target_link_libraries(LookoutTests
	PRIVATE
		GTest::gtest
		Tge::Core
		Tge::Testing
		Lkt::Browser
		Lkt::Config
		Lkt::Download
		Lkt::Games
		Lkt::Geo
		Lkt::Json
		Lkt::Launch
		Lkt::Net
		Lkt::Query
		Lkt::Script
		mbedtls
)

add_test(NAME LookoutTests COMMAND LookoutTests)
