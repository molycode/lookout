file(GLOB_RECURSE LKT_GEO_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktGeo STATIC ${LKT_GEO_SOURCES})

target_include_directories(LktGeo
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktGeo PRIVATE Tge::Base)

LktEmbedFile(LktGeo Ipv4Countries ${PROJECT_SOURCE_DIR}/assets/geo/ipv4-countries.bin)
