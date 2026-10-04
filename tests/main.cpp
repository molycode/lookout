#include "tge_environment.hpp"
#include "query/builtin_games.hpp"
#include "query/game_catalog.hpp"
#include <gtest/gtest.h>

//////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	Lkt::Query::InitializeGameCatalog(Lkt::Query::GetBuiltinGames());
	testing::InitGoogleTest(&argc, argv);
	testing::AddGlobalTestEnvironment(new Lkt::Fixtures::CTgeEnvironment{});

	int const result{ RUN_ALL_TESTS() };

	Lkt::Query::TerminateGameCatalog();

	return result;
}
