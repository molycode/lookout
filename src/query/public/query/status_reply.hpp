#pragma once

#include "query/player.hpp"
#include "query/rule.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace Lkt::Query
{
struct SStatusReply final
{
	std::vector<SRule> rules;
	std::vector<SPlayer> players;
	uint32_t numMalformedPlayerLines{ 0 };
};

// An empty view when the server does not publish the rule.
std::string_view FindRule(SStatusReply const& reply, std::string_view key);
} // namespace Lkt::Query
