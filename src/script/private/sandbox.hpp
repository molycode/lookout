#pragma once

#include "lua_api.hpp"

namespace Lkt::Script
{
// Called through Lua, with an empty stack: opens the allowed libraries and returns the sandbox environment.
int OpenSandbox(lua_State* pState);
} // namespace Lkt::Script
