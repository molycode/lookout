file(GLOB_RECURSE LKT_GAMES_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktGames STATIC ${LKT_GAMES_SOURCES})

target_include_directories(LktGames
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktGames
	PUBLIC
		Lkt::Query
		Tge::Threading
	PRIVATE
		Lkt::Json
		Lkt::Script
		Tge::Logging
)

LktEmbedFile(LktGames NewGame ${PROJECT_SOURCE_DIR}/assets/new-game.json)
