#pragma once

#include "launch/launch_error.hpp"
#include "launch/launch_option.hpp"
#include "query/game.hpp"
#include <expected>
#include <string_view>

namespace Lkt::Launch
{
bool IsFolderInstallSupported(Query::EGame game);
// Logs why a folder cannot start the game.
std::expected<SLaunchOption, ELaunchError> MakeFolderOption(Query::EGame game, std::string_view folder);
} // namespace Lkt::Launch
