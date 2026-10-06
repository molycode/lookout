#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Download
{
struct SIndexFile;

std::expected<std::optional<std::string>, std::string> ReadCachedIcon(std::filesystem::path const& iconDir, SIndexFile const& icon);
std::expected<void, std::string> CacheIcon(std::filesystem::path const& iconDir, std::string_view sha256, std::string_view png);
std::vector<std::string> PruneIconCache(std::filesystem::path const& iconDir, std::set<std::string, std::less<>> const& iconHashes);
} // namespace Lkt::Download
