#include "tge_environment.hpp"
#include "games/load_games.hpp"
#include "query/game_catalog.hpp"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>

//////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	Lkt::Games::SGameContent const builtins{ Lkt::Games::LoadGames({}) };
	int result{ EXIT_FAILURE };

	for (Lkt::Query::SGameProblem const& problem : builtins.problems)
	{
		std::fprintf(stderr, "%s\n", problem.text.c_str());
	}

	if (builtins.problems.empty())
	{
		Lkt::Query::InitializeGameCatalog(builtins.protocols, builtins.games);
		testing::InitGoogleTest(&argc, argv);
		testing::AddGlobalTestEnvironment(new Lkt::Fixtures::CTgeEnvironment{});
		result = RUN_ALL_TESTS();
		Lkt::Query::TerminateGameCatalog();
	}

	return result;
}
