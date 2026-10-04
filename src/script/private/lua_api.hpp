#pragma once

// Lua is C, and its git repository ships no lua.hpp.
extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
