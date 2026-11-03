#include "tge_environment.hpp"
#include "games/load_games.hpp"
#include "net/crypto_threading.hpp"
#include "query/game_catalog.hpp"
#include "query/store_catalog.hpp"
#include <gtest/gtest.h>
#include <tge/net/socket_library.hpp>
#include <cstdio>
#include <cstdlib>

//////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	Lkt::Net::CCryptoThreading cryptoThreading{};
	Tge::Net::CSocketLibrary socketLibrary{};

	cryptoThreading.Initialize();

	bool const areSocketsReady{ socketLibrary.Initialize().has_value() };
	Lkt::Games::SGameContent const downloaded{ Lkt::Games::LoadGames(LKT_LOOKOUT_GAMES_DIR, {}) };
	int result{ EXIT_FAILURE };

	for (Lkt::Query::SGameProblem const& problem : downloaded.problems)
	{
		std::fprintf(stderr, "%s\n", problem.text.c_str());
	}

	if (!areSocketsReady)
	{
		std::fprintf(stderr, "Cannot start the socket library\n");
	}

	if (areSocketsReady && downloaded.problems.empty())
	{
		Lkt::Query::InitializeGameCatalog(downloaded.protocols, downloaded.games);
		Lkt::Query::InitializeStoreCatalog(downloaded.stores);
		testing::InitGoogleTest(&argc, argv);
		testing::AddGlobalTestEnvironment(new Lkt::Fixtures::CTgeEnvironment{});
		result = RUN_ALL_TESTS();
		Lkt::Query::TerminateStoreCatalog();
		Lkt::Query::TerminateGameCatalog();
	}

	socketLibrary.Terminate();
	cryptoThreading.Terminate();

	return result;
}
