file(GLOB_RECURSE LKT_CONFIG_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktConfig STATIC ${LKT_CONFIG_SOURCES})

target_include_directories(LktConfig
	PUBLIC public
	PRIVATE private
)

# SYSTEM, so -Werror never applies to nlohmann's own code.
target_include_directories(LktConfig SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/external/json/single_include)

target_link_libraries(LktConfig
	PUBLIC
		Lkt::Query
		Tge::Base
	PRIVATE
		Tge::Logging
)
