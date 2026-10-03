file(GLOB_RECURSE LKT_QUERY_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktQuery STATIC ${LKT_QUERY_SOURCES})

target_include_directories(LktQuery
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktQuery PUBLIC Tge::Base)
