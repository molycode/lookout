#pragma once

// Lua is C, and its git repository ships no lua.hpp.
extern "C"
{
#include <lauxlib.h> // IWYU pragma: export
#include <lua.h> // IWYU pragma: export
#include <lualib.h> // IWYU pragma: export
}
