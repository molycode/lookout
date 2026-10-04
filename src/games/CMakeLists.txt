file(GLOB_RECURSE LKT_GAMES_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktGames STATIC ${LKT_GAMES_SOURCES})

target_include_directories(LktGames
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktGames
	PUBLIC
		Lkt::Query
	PRIVATE
		Lkt::Json
		Tge::Logging
)

LktEmbedFile(LktGames KingpinGame ${PROJECT_SOURCE_DIR}/assets/games/kingpin/game.json)
LktEmbedFile(LktGames Quake2Game ${PROJECT_SOURCE_DIR}/assets/games/quake2/game.json)
LktEmbedFile(LktGames RtcwGame ${PROJECT_SOURCE_DIR}/assets/games/rtcw/game.json)
LktEmbedFile(LktGames EnemyTerritoryGame ${PROJECT_SOURCE_DIR}/assets/games/et/game.json)
LktEmbedFile(LktGames Quake3Game ${PROJECT_SOURCE_DIR}/assets/games/quake3/game.json)
