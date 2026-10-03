file(GLOB_RECURSE LKT_BROWSER_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktBrowser STATIC ${LKT_BROWSER_SOURCES})

target_include_directories(LktBrowser
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktBrowser
	PUBLIC
		Lkt::Config
		Lkt::Geo
		Lkt::Launch
		Lkt::Net
		Lkt::Query
	PRIVATE
		Tge::Logging
)
