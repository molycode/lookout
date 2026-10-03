#pragma once

#include "config/game_install.hpp"
#include "config/server_filter.hpp"
#include "config/sort_order.hpp"
#include "query/server_address.hpp"
#include <vector>

namespace Lkt::Config
{
struct SGameSettings final
{
	bool isListed{ true };
	SServerFilter filter;
	SSortOrder sort;
	std::vector<SGameInstall> installs;
	std::vector<Query::SServerAddress> favourites;

	bool operator==(SGameSettings const&) const = default;
};
} // namespace Lkt::Config
