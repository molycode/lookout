#pragma once

#include "query/game.hpp"
#include <span>
#include <string_view>

namespace Lkt::Launch
{
// Desktop-file ids in order of preference; installDir is relative to $HOME, and the other paths to installDir.
struct SLaunchHints final
{
	Query::EGame game{ Query::EGame::Kingpin };
	std::span<std::string_view const> desktopFiles;
	std::string_view installDir;
	std::string_view program;
	std::span<std::string_view const> requiredFiles;
};

SLaunchHints const& GetLaunchHints(Query::EGame game);
} // namespace Lkt::Launch
