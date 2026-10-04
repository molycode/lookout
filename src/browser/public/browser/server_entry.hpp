#pragma once

#include "browser/server_state.hpp"
#include "geo/countries.hpp"
#include "query/server_address.hpp"
#include "query/server_summary.hpp"
#include "query/status_reply.hpp"
#include "query/styled_text.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Lkt::Browser
{
struct SServerEntry final
{
	Query::SServerAddress address;
	Query::SServerAddress joinAddress;
	EServerState state{ EServerState::Pending };
	uint32_t pingMs{ 0 };
	uint8_t country{ Geo::NoCountry };
	Query::SServerSummary summary;
	Query::SStatusReply reply;
	std::vector<Query::SStyledText> playerNames;
	std::vector<std::string> playerFieldKeys;
	std::string searchText;
	bool isFavourite{ false };
	bool isStale{ false };
};
} // namespace Lkt::Browser
