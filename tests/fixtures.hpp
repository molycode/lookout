#pragma once

#include "query/game.hpp"
#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
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

// A game of lookout-games by its key; an unknown key fails the calling test and stops the run.
Query::SGameDefinition const& GetGameByKey(std::string_view key);
Query::EGame GetGameId(std::string_view key);

// A protocol of lookout-games by its name; an unknown name fails the calling test and stops the run.
Query::SProtocolDefinition const& GetProtocolByName(std::string_view name);
} // namespace Lkt::Fixtures
