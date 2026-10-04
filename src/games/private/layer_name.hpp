#pragma once

#include <filesystem>
#include <string>

namespace Lkt::Games
{
// How problems name a folder of games: by its last component, also when its path ends in a separator.
std::string NameLayer(std::filesystem::path const& folder);
} // namespace Lkt::Games
