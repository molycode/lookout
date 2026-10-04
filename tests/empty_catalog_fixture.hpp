#pragma once

#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
#include <gtest/gtest.h>
#include <vector>

namespace Lkt::Fixtures
{
// Empties the game catalog for one test, as on a first start with nothing installed, and puts it back afterwards.
class CEmptyCatalogTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override;
	void TearDown() override;
	// ~testing::Test

private:

	std::vector<Query::SProtocolDefinition> m_protocols;
	std::vector<Query::SGameDefinition> m_games;
};
} // namespace Lkt::Fixtures
