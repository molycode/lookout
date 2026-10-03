#include "launch/launch_discovery.hpp"
#include "command_line.hpp"
#include "desktop_entry.hpp"
#include "hinted_discovery.hpp"
#include "install_folder.hpp"
#include "launch/home_path.hpp"
#include "launch/launcher_ids.hpp"
#include "loggers.hpp"
#include "program_path.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <algorithm>
#include <cerrno>
#include <expected>
#include <fstream>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace Lkt::Launch
{
namespace
{
constexpr std::string_view ApplicationsDir{ "applications" };

//////////////////////////////////////////////////////////////////////////
// The first directory holding the id wins, so a user's copy, or a Hidden=true one, overrides the system's.
std::optional<std::filesystem::path> LocateDesktopFile(std::string_view id, SLaunchEnvironment const& environment)
{
	std::optional<std::filesystem::path> located{};
	std::vector<std::filesystem::path> directories{};

	if (!environment.dataHome.empty())
	{
		directories.emplace_back(environment.dataHome);
	}

	directories.insert(directories.end(), environment.dataDirs.begin(), environment.dataDirs.end());

	for (size_t index{ 0 }; !located.has_value() && index < directories.size(); ++index)
	{
		std::filesystem::path const candidate{ directories[index] / ApplicationsDir / id };
		std::error_code error{};

		if (std::filesystem::exists(candidate, error))
		{
			located = candidate;
		}
		else if (error.value() != 0)
		{
			gLog.Warning("Cannot look for the launcher '{}': {}", candidate.string(), error.message());
		}
	}

	return located;
}

//////////////////////////////////////////////////////////////////////////
// A directory makes the stream throw and a FIFO makes it block, and without exceptions a throw ends Lookout.
std::expected<std::string, std::string> ReadText(std::filesystem::path const& path)
{
	std::expected<std::string, std::string> result{ std::unexpected{ std::string{ "it is not a regular file" } } };
	std::error_code error{};

	if (std::filesystem::is_regular_file(path, error))
	{
		std::ifstream file{ path, std::ios::binary };
		int const openError{ errno };

		if (file.is_open())
		{
			std::string text{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

			if (file.bad())
			{
				result = std::unexpected{ std::string{ "it cannot be read" } };
			}
			else
			{
				result = std::move(text);
			}
		}
		else
		{
			result = std::unexpected{ std::format("it cannot be opened: {}", std::generic_category().message(openError)) };
		}
	}
	else if (error.value() != 0)
	{
		result = std::unexpected{ std::format("it cannot be examined: {}", error.message()) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// The reason is a phrase for "'<file>' is skipped: <reason>".
std::expected<SLaunchOption, std::string> MakeDesktopOption(std::string_view id, std::filesystem::path const& file, SDesktopEntry const& entry, SLaunchEnvironment const& environment)
{
	std::expected<SLaunchOption, std::string> result{ std::unexpected{ std::string{} } };
	std::filesystem::path const workingDir{ entry.path };
	std::expected<std::vector<std::string>, ECommandLineError> const split{ SplitCommandLine(entry.exec, EQuoting::Lenient) };
	std::expected<std::vector<std::string>, ECommandLineError> const argv{ split.and_then([&entry, &file](std::vector<std::string> const& arguments)
	{
		return ExpandFieldCodes(arguments, entry.name, file.string());
	}) };

	if (entry.type.empty())
	{
		result = std::unexpected{ std::string{ "it has no Type" } };
	}
	else if (entry.type != "Application")
	{
		result = std::unexpected{ std::format("its Type is '{}', not Application", entry.type) };
	}
	else if (!workingDir.empty() && workingDir.is_relative())
	{
		result = std::unexpected{ std::format("its Path '{}' is not absolute", entry.path) };
	}
	else if (!split.has_value())
	{
		result = std::unexpected{ std::format("its Exec cannot be split: {}", ToString(split.error())) };
	}
	else if (!argv.has_value())
	{
		result = std::unexpected{ std::format("its Exec cannot be expanded: {}", ToString(argv.error())) };
	}
	else if (argv->empty())
	{
		result = std::unexpected{ std::string{ "its Exec is empty" } };
	}
	else
	{
		std::expected<std::filesystem::path, EProgramError> const tryExec{ entry.tryExec.empty()
			? std::expected<std::filesystem::path, EProgramError>{}
			: ResolveProgram(entry.tryExec, workingDir, environment.searchPath) };
		std::expected<std::filesystem::path, EProgramError> const program{ ResolveProgram(argv->front(), workingDir, environment.searchPath) };

		if (!tryExec.has_value())
		{
			result = std::unexpected{ std::format("its TryExec program '{}' {}", entry.tryExec, ToString(tryExec.error())) };
		}
		else if (!program.has_value())
		{
			result = std::unexpected{ std::format("its program '{}' {}", argv->front(), ToString(program.error())) };
		}
		else
		{
			std::vector<std::string> arguments{ *argv };
			std::string const folder{ entry.path.empty() ? program->parent_path().string() : entry.path };

			arguments.front() = program->string();
			result = SLaunchOption{ std::string{ id }, entry.name.empty() ? std::string{ id } : entry.name, ShortenHome(folder, environment.home),
				std::move(arguments), entry.path };
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Hidden=true is the specification's "deleted", so it hides the entry without a word.
std::optional<SLaunchOption> FindDesktopOption(std::string_view id, SLaunchEnvironment const& environment)
{
	std::optional<SLaunchOption> option{};
	std::optional<std::filesystem::path> const file{ LocateDesktopFile(id, environment) };

	if (file.has_value())
	{
		std::expected<std::string, std::string> const text{ ReadText(*file) };
		std::expected<SDesktopEntry, EDesktopEntryError> entry{ std::unexpected{ EDesktopEntryError::NoDesktopEntryGroup } };

		if (text.has_value())
		{
			entry = ParseDesktopEntry(*text);
		}

		if (!text.has_value())
		{
			gLog.Warning("The launcher '{}' is skipped: {}", file->string(), text.error());
		}
		else if (!entry.has_value())
		{
			gLog.Warning("The launcher '{}' is skipped: {}", file->string(), ToString(entry.error()));
		}
		else if (!entry->isHidden)
		{
			std::expected<SLaunchOption, std::string> made{ MakeDesktopOption(id, *file, *entry, environment) };

			if (made.has_value())
			{
				option = std::move(*made);
			}
			else
			{
				gLog.Warning("The launcher '{}' is skipped: {}", file->string(), made.error());
			}
		}
	}

	return option;
}

//////////////////////////////////////////////////////////////////////////
std::optional<SLaunchOption> FindInstallOption(SLaunchHints const& hints, std::string_view gameName, SLaunchEnvironment const& environment)
{
	std::optional<SLaunchOption> option{};

	if (!hints.installDir.empty())
	{
		std::filesystem::path const folder{ environment.home / hints.installDir };
		std::error_code error{};

		if (environment.home.empty())
		{
			gLog.Warning("{}'s default folder ~/{} is not checked: HOME is unset or not an absolute path", gameName, hints.installDir);
		}
		else if (std::filesystem::is_directory(folder, error))
		{
			std::expected<std::filesystem::path, std::string> const program{ CheckInstallFolder(hints, folder) };

			if (program.has_value())
			{
				option = SLaunchOption{ std::string{ InstallDirLauncherId }, std::string{ gameName }, ShortenHome(folder.string(), environment.home),
					{ program->string() }, folder.string() };
			}
			else
			{
				gLog.Warning("'{}' is not used to start {}: {}", folder.string(), gameName, program.error());
			}
		}
		else if (error.value() != 0 && error != std::errc::no_such_file_or_directory)
		{
			gLog.Warning("Cannot look into '{}' for {}: {}", folder.string(), gameName, error.message());
		}
	}

	return option;
}

//////////////////////////////////////////////////////////////////////////
bool StartsTheSameProgram(SLaunchOption const& lhs, SLaunchOption const& rhs)
{
	std::error_code error{};

	return std::filesystem::equivalent(lhs.argv.front(), rhs.argv.front(), error) && error.value() == 0;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::vector<SLaunchOption> FindLaunchOptions(SLaunchHints const& hints, std::string_view gameName, SLaunchEnvironment const& environment)
{
	std::vector<SLaunchOption> options{};

	for (std::string_view const id : hints.desktopFiles)
	{
		std::optional<SLaunchOption> option{ FindDesktopOption(id, environment) };

		if (option.has_value())
		{
			options.emplace_back(std::move(*option));
		}
	}

	std::optional<SLaunchOption> installOption{ FindInstallOption(hints, gameName, environment) };

	if (installOption.has_value() && std::ranges::none_of(options, [&installOption](SLaunchOption const& option) { return StartsTheSameProgram(option, *installOption); }))
	{
		options.emplace_back(std::move(*installOption));
	}

	return options;
}

//////////////////////////////////////////////////////////////////////////
std::vector<SLaunchOption> FindLaunchOptions(Query::EGame game, SLaunchEnvironment const& environment)
{
	return FindLaunchOptions(GetLaunchHints(game), Query::GetGame(game).name, environment);
}
} // namespace Lkt::Launch
