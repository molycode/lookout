#pragma once

#include "key_values.hpp"
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace Lkt::Launch
{
std::expected<std::vector<SKeyValue>, std::string> ReadSteamFile(std::filesystem::path const& path);
} // namespace Lkt::Launch
