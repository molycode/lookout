#include "sandbox.hpp"
#include <array>
#include <cstddef>

namespace Lkt::Script
{
namespace
{
constexpr std::array<char const*, 9> BaseNames{ "assert", "error", "ipairs", "next", "pairs", "select", "tonumber", "tostring", "type" };

// No match, gmatch or gsub: one crafted reply keeps a backtracking pattern busy for seconds, beyond any deadline.
constexpr std::array<char const*, 12> StringNames{ "byte", "char", "format", "len", "lower", "upper", "rep", "reverse", "sub", "pack",
	"packsize", "unpack" };

// No sort: it runs entirely in C, where the deadline cannot stop it.
constexpr std::array<char const*, 5> TableNames{ "concat", "insert", "move", "remove", "unpack" };

//////////////////////////////////////////////////////////////////////////
// The original find is the closure's upvalue; plain is forced whatever the script passed.
int FindPlain(lua_State* pState)
{
	lua_settop(pState, 3);
	lua_pushboolean(pState, 1);
	lua_pushvalue(pState, lua_upvalueindex(1));
	lua_insert(pState, 1);
	lua_call(pState, 4, LUA_MULTRET);

	return lua_gettop(pState);
}

//////////////////////////////////////////////////////////////////////////
template<size_t NumNames>
void PushSubset(lua_State* pState, int library, std::array<char const*, NumNames> const& names)
{
	lua_createtable(pState, 0, static_cast<int>(NumNames));

	for (char const* const pName : names)
	{
		lua_getfield(pState, library, pName);
		lua_setfield(pState, -2, pName);
	}
}

//////////////////////////////////////////////////////////////////////////
void PushCopy(lua_State* pState, int library)
{
	lua_newtable(pState);
	lua_pushnil(pState);

	while (lua_next(pState, library) != 0)
	{
		lua_pushvalue(pState, -2);
		lua_insert(pState, -2);
		lua_rawset(pState, -4);
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
int OpenSandbox(lua_State* pState)
{
	constexpr int Base{ 1 };
	constexpr int String{ 2 };
	constexpr int Table{ 3 };
	constexpr int Math{ 4 };
	constexpr int Utf8{ 5 };
	constexpr int Environment{ 6 };
	constexpr int SandboxString{ 7 };

	luaL_requiref(pState, LUA_GNAME, luaopen_base, 0);
	luaL_requiref(pState, LUA_STRLIBNAME, luaopen_string, 0);
	luaL_requiref(pState, LUA_TABLIBNAME, luaopen_table, 0);
	luaL_requiref(pState, LUA_MATHLIBNAME, luaopen_math, 0);
	luaL_requiref(pState, LUA_UTF8LIBNAME, luaopen_utf8, 0);

	PushSubset(pState, Base, BaseNames);
	PushSubset(pState, String, StringNames);
	lua_getfield(pState, String, "find");
	lua_pushcclosure(pState, FindPlain, 1);
	lua_setfield(pState, SandboxString, "find");

	// Every string shares this metatable, so ("x"):match would otherwise reach the full library.
	lua_pushliteral(pState, "");
	lua_getmetatable(pState, -1);
	lua_pushvalue(pState, SandboxString);
	lua_setfield(pState, -2, "__index");
	lua_pop(pState, 2);

	lua_setfield(pState, Environment, LUA_STRLIBNAME);
	PushSubset(pState, Table, TableNames);
	lua_setfield(pState, Environment, LUA_TABLIBNAME);
	PushCopy(pState, Math);
	lua_setfield(pState, Environment, LUA_MATHLIBNAME);
	PushCopy(pState, Utf8);
	lua_setfield(pState, Environment, LUA_UTF8LIBNAME);

	return 1;
}
} // namespace Lkt::Script
