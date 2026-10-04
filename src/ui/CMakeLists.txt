set(lktImguiRoot ${PROJECT_SOURCE_DIR}/external/imgui)

add_library(imgui STATIC
	${lktImguiRoot}/imgui.cpp
	${lktImguiRoot}/imgui_draw.cpp
	${lktImguiRoot}/imgui_tables.cpp
	${lktImguiRoot}/imgui_widgets.cpp
	${lktImguiRoot}/misc/cpp/imgui_stdlib.cpp
	${lktImguiRoot}/backends/imgui_impl_sdl3.cpp
	${lktImguiRoot}/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(imgui SYSTEM PUBLIC ${lktImguiRoot} ${lktImguiRoot}/backends ${lktImguiRoot}/misc/cpp)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_DEFAULT_FONT)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)
tge_suppress_external_warnings(imgui)

file(GLOB_RECURSE LKT_UI_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktUi STATIC ${LKT_UI_SOURCES})

target_include_directories(LktUi
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktUi
	PUBLIC
		Lkt::Config
		Tge::Base
	PRIVATE
		Lkt::Browser
		Lkt::Download
		Lkt::Games
		Lkt::Geo
		Lkt::Launch
		Lkt::Query
		imgui
		SDL3::SDL3-static
		Tge::Lifecycle
		Tge::Logging
)

LktEmbedFile(LktUi RobotoMedium ${lktImguiRoot}/misc/fonts/Roboto-Medium.ttf)
LktEmbedFile(LktUi FontAwesomeSolid ${PROJECT_SOURCE_DIR}/assets/fonts/fa-solid-900.ttf)
LktEmbedFile(LktUi LookoutLicence ${PROJECT_SOURCE_DIR}/LICENSE)
LktEmbedFile(LktUi ImGuiLicence ${lktImguiRoot}/LICENSE.txt)
LktEmbedFile(LktUi SdlLicence ${PROJECT_SOURCE_DIR}/external/sdl/LICENSE.txt)
LktEmbedFile(LktUi SdlBundledLicences ${PROJECT_SOURCE_DIR}/assets/licences/sdl-bundled.txt)
LktEmbedFile(LktUi SdlWaylandLicences ${PROJECT_SOURCE_DIR}/assets/licences/sdl-wayland-protocols.txt)
LktEmbedFile(LktUi JsonLicence ${PROJECT_SOURCE_DIR}/external/json/LICENSE.MIT)
LktEmbedFile(LktUi JsonBundledLicences ${PROJECT_SOURCE_DIR}/assets/licences/json-bundled.txt)
LktEmbedFile(LktUi LuaLicence ${PROJECT_SOURCE_DIR}/assets/licences/lua.txt)
LktEmbedFile(LktUi MbedTlsLicence ${PROJECT_SOURCE_DIR}/external/mbedtls/LICENSE)
LktEmbedFile(LktUi TgeCoreLicence ${PROJECT_SOURCE_DIR}/external/tge-core/LICENSE)
LktEmbedFile(LktUi RpmallocLicence ${PROJECT_SOURCE_DIR}/external/tge-core/external/rpmalloc/LICENSE)
LktEmbedFile(LktUi RobotoLicence ${PROJECT_SOURCE_DIR}/assets/fonts/LICENSE-Roboto.txt)
LktEmbedFile(LktUi FontAwesomeLicence ${PROJECT_SOURCE_DIR}/assets/fonts/LICENSE-FontAwesome.txt)
LktEmbedFile(LktUi DbipLicence ${PROJECT_SOURCE_DIR}/assets/licences/dbip-country-lite.txt)
LktEmbedFile(LktUi FlagIconsLicence ${PROJECT_SOURCE_DIR}/assets/licences/flag-icons.txt)
LktEmbedFile(LktUi Flags ${PROJECT_SOURCE_DIR}/assets/geo/flags.png)
