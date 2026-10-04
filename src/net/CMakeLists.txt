file(GLOB_RECURSE LKT_NET_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktNet STATIC ${LKT_NET_SOURCES})

target_include_directories(LktNet
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktNet
	PUBLIC
		Lkt::Query
	PRIVATE
		Lkt::Script
		mbedtls
		Tge::Logging
		Tge::Threading
)
