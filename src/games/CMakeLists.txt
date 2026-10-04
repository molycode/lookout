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
		Lkt::Script
)

LktEmbedTree(LktGames Protocols ${PROJECT_SOURCE_DIR}/assets/protocols "*.lua")
LktEmbedTree(LktGames Games ${PROJECT_SOURCE_DIR}/assets/games "*/game.json")
LktEmbedTree(LktGames GameIcons ${PROJECT_SOURCE_DIR}/assets/games "*/icon.png")
