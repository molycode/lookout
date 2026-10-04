#include "app_dir_name.hpp"
#include "lookout.hpp"
#include "loggers.hpp"
#include "config/xdg_paths.hpp"
#include "games/check_folder.hpp"
#include "games/game_files.hpp"
#include "games/load_games.hpp"
#include "query/game_catalog.hpp"
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

	return keys.empty() ? std::string{ "none, as no game is installed (--download gets them)" } : keys;
}

//////////////////////////////////////////////////////////////////////////
void PrintUsage(std::string_view executable)
{
	std::println("Usage: {} [options]", executable);
	std::println("Options:");
	std::println("  --list <game>         Print the game's servers and exit; <game> is one of: {}", GetGameKeys());
	std::println("  --download [game...]  Download these games from lookout-games, or each one missing or with an update, and exit");
	std::println("  --check <folder>      Check the games and protocols of a lookout-games folder, print each problem and exit");
	std::println("  --version             Show the version");
	std::println("  --help                Show this help message");
}

//////////////////////////////////////////////////////////////////////////
// Everything on stdout, so a pull request's check log reads in order.
int CheckFolder(std::filesystem::path const& folder)
{
	Lkt::Games::SGameContent const content{ Lkt::Games::CheckGameFolder(folder) };
	size_t const numProblems{ content.problems.size() };

	for (Lkt::Query::SGameProblem const& problem : content.problems)
	{
		std::println("{}", problem.text);
	}

	std::println("{} games, {} protocols, {} {}", content.games.size(), content.protocols.size(), (numProblems == 0) ? std::string{ "no" }
		: std::to_string(numProblems), (numProblems == 1) ? "problem" : "problems");

	return (numProblems == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	std::expected<std::filesystem::path, Lkt::Config::EXdgError> const dataHome{ Lkt::Config::GetDataHome() };
	std::filesystem::path const userDir{ dataHome.has_value() ? *dataHome / Lkt::AppDirName : std::filesystem::path{} };
	Lkt::Games::SGameContent content{ Lkt::Games::LoadGames(Lkt::Games::GetDownloadedDir(userDir), userDir) };

	if (!dataHome.has_value())
	{
		content.problems.emplace_back(Lkt::Query::SGameProblem{ std::format("Cannot locate the data directory, so no games can be loaded: {}",
			Lkt::Config::ToString(dataHome.error())), {} });
	}

	Lkt::Query::InitializeGameCatalog(content.protocols, content.games);

	std::span<char* const> const args{ argv, static_cast<size_t>(argc) };
	std::string_view const executable{ args.empty() ? "lookout" : args.front() };
	std::span<char* const> const options{ args.empty() ? args : args.subspan(1) };

	bool valid{ true };
	bool showHelp{ false };
	bool showVersion{ false };
	bool isExpectingGame{ false };
	bool isExpectingFolder{ false };
	Lkt::SRunRequest request{};
	std::optional<std::filesystem::path> checkFolder{};

	for (std::string_view const arg : options)
	{
		if (valid)
		{
			if (isExpectingGame)
			{
				request.pListGame = Lkt::Query::FindGame(arg);
				valid = request.pListGame != nullptr;
				isExpectingGame = false;

				if (!valid)
				{
					Lkt::gLog.Error("Unknown game '{}' - expected one of: {}", arg, GetGameKeys());
				}
			}
			else if (isExpectingFolder)
			{
				checkFolder = std::filesystem::path{ arg };
				isExpectingFolder = false;
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
			else if (arg == "--check")
			{
				isExpectingFolder = true;
			}
			else if (arg == "--download")
			{
				request.downloadKeys = std::vector<std::string>{};
			}
			else if (request.downloadKeys.has_value() && !arg.starts_with("--"))
			{
				request.downloadKeys->emplace_back(arg);
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
	else if (valid && isExpectingFolder)
	{
		Lkt::gLog.Error("--check needs the folder to check");
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
	else if (valid && checkFolder.has_value())
	{
		result = CheckFolder(*checkFolder);
	}
	else if (valid)
	{
		Lkt::CLookout lookout;

		result = lookout.Run(request, userDir, content.problems) ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	Lkt::Query::TerminateGameCatalog();

	return result;
}
