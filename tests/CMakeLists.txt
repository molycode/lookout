file(GLOB_RECURSE LKT_TEST_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp")
list(FILTER LKT_TEST_SOURCES EXCLUDE REGEX "/fake_game/")

add_subdirectory(fake_game)

LktApp(LookoutTests ${LKT_TEST_SOURCES})
LktAddWindowsManifest(LookoutTests)

target_compile_definitions(LookoutTests PRIVATE LKT_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
	LKT_TEST_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scripts" LKT_TEST_GAMES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/games"
	LKT_LOOKOUT_GAMES_DIR="${PROJECT_SOURCE_DIR}/external/lookout-games" LKT_FAKE_GAME="$<TARGET_FILE:LookoutFakeGame>")

# Nothing else names the fake game, so building the tests has to build it.
add_dependencies(LookoutTests LookoutFakeGame)

# Pure logic private to its module by design, such as the network scheduler or the settings reader; reaching in
# beats widening an interface for a test.
target_include_directories(LookoutTests
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		$<TARGET_PROPERTY:LktBrowser,SOURCE_DIR>/private
		$<TARGET_PROPERTY:LktCli,SOURCE_DIR>/private
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
		Tge::Net
		Tge::Testing
		Lkt::Browser
		Lkt::Cli
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
		$<$<PLATFORM_ID:Windows>:iphlpapi>
)

add_test(NAME LookoutTests COMMAND LookoutTests)
