#pragma once

#include "query/game.hpp"
#include "query/game_definition.hpp"
#include "query/protocol.hpp"
#include "query/protocol_definition.hpp"
#include <gtest/gtest.h>
#include <cstddef>
#include <string_view>
#include <vector>

namespace Lkt::Fixtures
{
// Adds test protocols and games to the catalog for one test, after the built-ins so their ids stay, and puts the
// catalog back afterwards. An engine reads the catalog when it initializes, so add before that.
class CCatalogTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override;
	void TearDown() override;
	// ~testing::Test

	// tests/scripts/<name>.lua.
	Query::EProtocol AddProtocol(std::string_view name);
	// Under a key no other game has.
	Query::EGame AddGame(Query::SGameDefinition game);

private:

	void Install();

	std::vector<Query::SProtocolDefinition> m_protocols;
	std::vector<Query::SGameDefinition> m_games;
	size_t m_numBuiltinProtocols{ 0 };
	size_t m_numBuiltinGames{ 0 };
};
} // namespace Lkt::Fixtures
