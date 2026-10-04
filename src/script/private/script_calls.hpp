#pragma once

#include "lua_api.hpp"

namespace Lkt::Script
{
// Each runs under lua_pcall with its call struct as the only argument, a light userdata. A Lua error unwinds them
// with longjmp, so they keep no local with a destructor; results go into the call struct.
int LoadScript(lua_State* pState);
int CallRequest(lua_State* pState);
int CallParseMasterReply(lua_State* pState);
int CallParseStatusReply(lua_State* pState);
} // namespace Lkt::Script
