set(lktLuaRoot ${PROJECT_SOURCE_DIR}/external/lua)

# Only what the sandbox opens: no linit (it pulls in every library), io, os, package, debug or coroutines.
add_library(lua STATIC
	${lktLuaRoot}/lapi.c
	${lktLuaRoot}/lcode.c
	${lktLuaRoot}/lctype.c
	${lktLuaRoot}/ldebug.c
	${lktLuaRoot}/ldo.c
	${lktLuaRoot}/ldump.c
	${lktLuaRoot}/lfunc.c
	${lktLuaRoot}/lgc.c
	${lktLuaRoot}/llex.c
	${lktLuaRoot}/lmem.c
	${lktLuaRoot}/lobject.c
	${lktLuaRoot}/lopcodes.c
	${lktLuaRoot}/lparser.c
	${lktLuaRoot}/lstate.c
	${lktLuaRoot}/lstring.c
	${lktLuaRoot}/ltable.c
	${lktLuaRoot}/ltm.c
	${lktLuaRoot}/lundump.c
	${lktLuaRoot}/lvm.c
	${lktLuaRoot}/lzio.c
	${lktLuaRoot}/lauxlib.c
	${lktLuaRoot}/lbaselib.c
	${lktLuaRoot}/lstrlib.c
	${lktLuaRoot}/ltablib.c
	${lktLuaRoot}/lmathlib.c
	${lktLuaRoot}/lutf8lib.c
)

# Compiled as C, as upstream builds it: as C++ its errors would be exceptions, which Lookout builds without.
set_target_properties(lua PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(lua SYSTEM PUBLIC ${lktLuaRoot})
target_compile_definitions(lua PRIVATE LUA_USE_LINUX $<$<CONFIG:Debug>:LUA_USE_APICHECK>)
target_link_libraries(lua PRIVATE m)
tge_suppress_external_warnings(lua)

file(GLOB_RECURSE LKT_SCRIPT_SOURCES CONFIGURE_DEPENDS "public/*.hpp" "private/*.hpp" "private/*.cpp")

LktLib(LktScript STATIC ${LKT_SCRIPT_SOURCES})

target_include_directories(LktScript
	PUBLIC public
	PRIVATE private
)

target_link_libraries(LktScript
	PUBLIC
		Lkt::Query
	PRIVATE
		lua
)
