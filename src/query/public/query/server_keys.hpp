#pragma once

#include <string>
#include <vector>

namespace Lkt::Query
{
// The server rules a game publishes each column under; mods lists candidates in order of preference.
struct SServerKeys final
{
	std::string hostname;
	std::string map;
	std::string maxPlayers;
	std::string password;
	std::vector<std::string> mods;
};
} // namespace Lkt::Query
