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
		Tge::Process
	PRIVATE
		Tge::IO
		Tge::Logging
		$<$<PLATFORM_ID:Windows>:advapi32>
		$<$<PLATFORM_ID:Windows>:ole32>
		$<$<PLATFORM_ID:Windows>:shell32>
)
