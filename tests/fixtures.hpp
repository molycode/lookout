#pragma once

#include <cstddef>
#include <filesystem>
#include <string_view>
#include <vector>

namespace Lkt::Fixtures
{
// Replies captured from live servers by tools/query.py --save-fixtures; an empty result fails the calling test.
std::vector<std::byte> LoadFixture(std::string_view relativePath);
std::vector<std::filesystem::path> ListFixtures(std::string_view game, std::string_view prefix);

std::vector<std::byte> ToBytes(std::string_view text);
} // namespace Lkt::Fixtures
