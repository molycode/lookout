#pragma once

#include <span>
#include <string_view>

namespace Lkt::Query
{
// The server rules a game publishes each column under; mods lists candidates in order of preference.
struct SServerKeys final
{
	std::string_view hostname;
	std::string_view map;
	std::string_view maxPlayers;
	std::string_view password;
	std::span<std::string_view const> mods;
};
} // namespace Lkt::Query
