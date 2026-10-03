file(GLOB_RECURSE LKT_LAUNCH_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktLaunch STATIC ${LKT_LAUNCH_SOURCES})

target_include_directories(LktLaunch
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktLaunch
	PUBLIC
		Lkt::Query
		Tge::Base
	PRIVATE
		Tge::Logging
)
