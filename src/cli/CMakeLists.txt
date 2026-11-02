file(GLOB_RECURSE LKT_CLI_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktCli STATIC ${LKT_CLI_SOURCES})

target_include_directories(LktCli
	PUBLIC public
	PRIVATE private
)

target_compile_definitions(LktCli PUBLIC LKT_VERSION="${PROJECT_VERSION}")

target_link_libraries(LktCli
	PUBLIC
		Lkt::Net
		Lkt::Query
		Tge::Base
		Tge::Net
	PRIVATE
		Lkt::Browser
		Lkt::Config
		Lkt::Download
		Lkt::Games
		Tge::IO
		Tge::Lifecycle
		Tge::Logging
)
