#pragma once

#include "query/game.hpp"
#include "query/game_definition.hpp"
#include "query/protocol.hpp"
#include "query/protocol_definition.hpp"
#include <span>
#include <string_view>

namespace Lkt::Query
{
// Filled before any thread starts and read-only until terminated; each game's and protocol's id is its position.
void InitializeGameCatalog(std::span<SProtocolDefinition const> protocols, std::span<SGameDefinition const> games);
void TerminateGameCatalog();

std::span<SGameDefinition const> GetGameCatalog();
SGameDefinition const& GetGame(EGame game);

// Null for a key no game has.
SGameDefinition const* FindGame(std::string_view key);

std::span<SProtocolDefinition const> GetProtocolCatalog();
SProtocolDefinition const& GetProtocol(EProtocol protocol);
} // namespace Lkt::Query
