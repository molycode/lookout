#pragma once

#include <filesystem>
#include <vector>

namespace Lkt::Launch
{
std::vector<std::filesystem::path> ReadSteamLibraries(std::filesystem::path const& steamFolder);
} // namespace Lkt::Launch
