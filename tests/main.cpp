#include "tge_environment.hpp"
#include "games/load_games.hpp"
#include "query/game_catalog.hpp"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>

//////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	Lkt::Games::SGameContent const downloaded{ Lkt::Games::LoadGames(LKT_LOOKOUT_GAMES_DIR, {}) };
	int result{ EXIT_FAILURE };

	for (Lkt::Query::SGameProblem const& problem : downloaded.problems)
	{
		std::fprintf(stderr, "%s\n", problem.text.c_str());
	}

	if (downloaded.problems.empty())
	{
		Lkt::Query::InitializeGameCatalog(downloaded.protocols, downloaded.games);
		testing::InitGoogleTest(&argc, argv);
		testing::AddGlobalTestEnvironment(new Lkt::Fixtures::CTgeEnvironment{});
		result = RUN_ALL_TESTS();
		Lkt::Query::TerminateGameCatalog();
	}

	return result;
}
