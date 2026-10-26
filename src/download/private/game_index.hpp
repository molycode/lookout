#pragma once

#include "index_game.hpp"
#include "index_protocol.hpp"
#include <string>
#include <vector>

namespace Lkt::Download
{
// lookout-games' index.json: the commit it describes, from which every file is fetched.
struct SGameIndex final
{
	std::string commit;
	std::vector<SIndexGame> games;
	std::vector<SIndexProtocol> protocols;
};
} // namespace Lkt::Download
