#pragma once

#include <string>
#include <vector>

namespace Lkt::Query
{
// The server rules a game publishes each column under; mods lists candidates in order of preference. Without
// numPlayers, the players are counted from the list.
struct SServerKeys final
{
	std::string hostname;
	std::string map;
	std::string numPlayers;
	std::string maxPlayers;
	std::string password;
	std::vector<std::string> mods;
};
} // namespace Lkt::Query
