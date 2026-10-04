#pragma once

#include "query/rule.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Lkt::Query
{
// The name as the server sent it; decode it with the game's text style before showing it. Fields hold whatever else
// the protocol reports about a player, in its order.
struct SPlayer final
{
	std::string name;
	std::optional<int32_t> score;
	std::optional<uint32_t> ping;
	std::vector<SRule> fields;
};
} // namespace Lkt::Query
