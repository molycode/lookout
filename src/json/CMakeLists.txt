file(GLOB_RECURSE LKT_JSON_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktJson STATIC ${LKT_JSON_SOURCES})

target_include_directories(LktJson
	PUBLIC public
	PRIVATE private
)

# SYSTEM, so -Werror never applies to nlohmann's own code.
target_include_directories(LktJson SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/external/json/single_include)

target_link_libraries(LktJson PRIVATE Tge::Logging)
