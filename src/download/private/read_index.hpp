#pragma once

#include "game_index.hpp"
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Download
{
// Strict: every name becomes a path, so a key, a file name or a commit that could leave the repository's layout fails
// the whole index. The error names where it went wrong ("index.json: games.kingpin.files: …").
std::expected<SGameIndex, std::string> ReadIndex(std::string_view text);
} // namespace Lkt::Download
