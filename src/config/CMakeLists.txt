file(GLOB_RECURSE LKT_CONFIG_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktConfig STATIC ${LKT_CONFIG_SOURCES})

target_include_directories(LktConfig
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktConfig
	PUBLIC
		Lkt::Query
		Tge::Base
	PRIVATE
		Lkt::Json
		Tge::Logging
)
