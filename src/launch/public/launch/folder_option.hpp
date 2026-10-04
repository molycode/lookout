#pragma once

#include "launch/launch_error.hpp"
#include "launch/launch_option.hpp"
#include <expected>
#include <string_view>

namespace Lkt
{
namespace Query
{
struct SGameDefinition;
} // namespace Query

namespace Launch
{
bool IsFolderInstallSupported(Query::SGameDefinition const& game);
// Logs why a folder cannot start the game.
std::expected<SLaunchOption, ELaunchError> MakeFolderOption(Query::SGameDefinition const& game, std::string_view folder);
} // namespace Launch
} // namespace Lkt
