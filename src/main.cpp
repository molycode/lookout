#include "lookout.hpp"
#include "loggers.hpp"
#include "games/builtin_games.hpp"
#include "query/game_catalog.hpp"
#include <cstdlib>
#include <print>
#include <span>
#include <string>
#include <string_view>

namespace
{
//////////////////////////////////////////////////////////////////////////
std::string GetGameKeys()
{
	std::string keys{};

	for (Lkt::Query::SGameDefinition const& game : Lkt::Query::GetGameCatalog())
	{
		keys += keys.empty() ? "" : ", ";
		keys += game.key;
	}

	return keys;
}

//////////////////////////////////////////////////////////////////////////
void PrintUsage(std::string_view executable)
{
	std::println("Usage: {} [options]", executable);
	std::println("Options:");
	std::println("  --list <game>  Print the game's servers and exit; <game> is one of: {}", GetGameKeys());
	std::println("  --version      Show the version");
	std::println("  --help         Show this help message");
}
} // namespace

//////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	Lkt::Games::SBuiltins const builtins{ Lkt::Games::LoadBuiltins() };

	Lkt::Query::InitializeGameCatalog(builtins.protocols, builtins.games);

	std::span<char* const> const args{ argv, static_cast<size_t>(argc) };
	std::string_view const executable{ args.empty() ? "lookout" : args.front() };
	std::span<char* const> const options{ args.empty() ? args : args.subspan(1) };

	bool valid{ true };
	bool showHelp{ false };
	bool showVersion{ false };
	bool isExpectingGame{ false };
	Lkt::Query::SGameDefinition const* pListGame{ nullptr };

	for (std::string_view const arg : options)
	{
		if (valid)
		{
			if (isExpectingGame)
			{
				pListGame = Lkt::Query::FindGame(arg);
				valid = pListGame != nullptr;
				isExpectingGame = false;

				if (!valid)
				{
					Lkt::gLog.Error("Unknown game '{}' - expected one of: {}", arg, GetGameKeys());
				}
			}
			else if (arg == "--help")
			{
				showHelp = true;
			}
			else if (arg == "--version")
			{
				showVersion = true;
			}
			else if (arg == "--list")
			{
				isExpectingGame = true;
			}
			else
			{
				Lkt::gLog.Error("Unknown option '{}' - try --help", arg);
				valid = false;
			}
		}
	}

	if (valid && isExpectingGame)
	{
		Lkt::gLog.Error("--list needs a game, one of: {}", GetGameKeys());
		valid = false;
	}

	int result{ EXIT_FAILURE };

	if (showHelp)
	{
		PrintUsage(executable);
		result = EXIT_SUCCESS;
	}
	else if (showVersion)
	{
		std::println("Lookout {}", LKT_VERSION);
		result = EXIT_SUCCESS;
	}
	else if (valid)
	{
		Lkt::CLookout lookout;

		result = lookout.Run(pListGame, builtins.problems) ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	Lkt::Query::TerminateGameCatalog();

	return result;
}
