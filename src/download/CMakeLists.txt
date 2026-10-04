file(GLOB_RECURSE LKT_DOWNLOAD_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktDownload STATIC ${LKT_DOWNLOAD_SOURCES})

target_include_directories(LktDownload
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktDownload
	PUBLIC
		Lkt::Net
	PRIVATE
		Lkt::Games
		Lkt::Json
		Lkt::Script
		mbedtls
		Tge::Logging
)
