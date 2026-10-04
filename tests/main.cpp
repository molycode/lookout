#include "tge_environment.hpp"
#include "games/builtin_games.hpp"
#include "query/game_catalog.hpp"
#include <gtest/gtest.h>

//////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	Lkt::Query::InitializeGameCatalog(Lkt::Games::LoadBuiltinGames());
	testing::InitGoogleTest(&argc, argv);
	testing::AddGlobalTestEnvironment(new Lkt::Fixtures::CTgeEnvironment{});

	int const result{ RUN_ALL_TESTS() };

	Lkt::Query::TerminateGameCatalog();

	return result;
}
