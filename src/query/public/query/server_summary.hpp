#pragma once

#include "query/game_definition.hpp"
#include "query/status_reply.hpp"
#include "query/styled_text.hpp"
#include <cstdint>
#include <string>

namespace Lkt::Query
{
struct SServerSummary final
{
	SStyledText name;
	std::string map;
	std::string mod;
	std::string mode;
	uint32_t numPlayers{ 0 };
	uint32_t maxPlayers{ 0 };
	bool hasPassword{ false };
	bool isForeign{ false };
};

SServerSummary Summarize(SGameDefinition const& game, SStatusReply const& reply);
} // namespace Lkt::Query
