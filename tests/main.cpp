#include "tge_environment.hpp"
#include "games/builtin_games.hpp"
#include "query/game_catalog.hpp"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <string>

//////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	Lkt::Games::SBuiltins const builtins{ Lkt::Games::LoadBuiltins() };
	int result{ EXIT_FAILURE };

	for (std::string const& problem : builtins.problems)
	{
		std::fprintf(stderr, "%s\n", problem.c_str());
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
