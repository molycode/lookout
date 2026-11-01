#include "parse_command_line.hpp"
#include "game_keys.hpp"
#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
#include <utility>
#include <vector>

namespace Lkt::Cli
{
//////////////////////////////////////////////////////////////////////////
// Reading stops at the first problem, so --help or --version wins only over what follows it.
std::expected<SRunRequest, std::string> ParseCommandLine(std::span<std::string_view const> options, std::span<std::string_view const> gameKeys)
{
	std::string problem{};
	bool showHelp{ false };
	bool showVersion{ false };
	bool isExpectingGame{ false };
	bool isExpectingFolder{ false };
	std::optional<std::string> listGame{};
	std::optional<std::vector<std::string>> downloadKeys{};
	std::optional<std::filesystem::path> checkFolder{};

	for (std::string_view const option : options)
	{
		if (problem.empty())
		{
			if (isExpectingGame)
			{
				isExpectingGame = false;

				if (std::ranges::contains(gameKeys, option))
				{
					listGame = std::string{ option };
				}
				else
				{
					problem = std::format("Unknown game '{}' - expected one of: {}", option, FormatGameKeys(gameKeys));
				}
			}
			else if (isExpectingFolder)
			{
				checkFolder = std::filesystem::path{ option };
				isExpectingFolder = false;
			}
			else if (option == "--help")
			{
				showHelp = true;
			}
			else if (option == "--version")
			{
				showVersion = true;
			}
			else if (option == "--list")
			{
				isExpectingGame = true;
			}
			else if (option == "--check")
			{
				isExpectingFolder = true;
			}
			else if (option == "--download")
			{
				downloadKeys = std::vector<std::string>{};
			}
			else if (downloadKeys.has_value() && !option.starts_with("--"))
			{
				downloadKeys->emplace_back(option);
			}
			else
			{
				problem = std::format("Unknown option '{}' - try --help", option);
			}
		}
	}

	if (problem.empty() && isExpectingGame)
	{
		problem = std::format("--list needs a game, one of: {}", FormatGameKeys(gameKeys));
	}
	else if (problem.empty() && isExpectingFolder)
	{
		problem = "--check needs the folder to check";
	}

	std::expected<SRunRequest, std::string> request{ SRunRequest{} };

	if (showHelp)
	{
		request->command = ECommand::Help;
	}
	else if (showVersion)
	{
		request->command = ECommand::Version;
	}
	else if (!problem.empty())
	{
		request = std::unexpected{ std::move(problem) };
	}
	else if (checkFolder.has_value())
	{
		request->command = ECommand::Check;
		request->checkFolder = std::move(*checkFolder);
	}
	else if (downloadKeys.has_value())
	{
		request->command = ECommand::Download;
		request->downloadKeys = std::move(*downloadKeys);
	}
	else if (listGame.has_value())
	{
		request->command = ECommand::List;
		request->listGame = std::move(*listGame);
	}

	return request;
}
} // namespace Lkt::Cli
