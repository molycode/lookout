#include "steam_source.hpp"
#include "install_folder.hpp"
#include "launch/launcher_ids.hpp"
#include "loggers.hpp"
#include "steam_file.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace Lkt::Launch
{
namespace
{
// The bit of StateFlags Steam sets once a game is fully installed; others mark downloads and updates.
constexpr uint32_t FullyInstalled{ 4 };

//////////////////////////////////////////////////////////////////////////
std::string_view FindValue(std::vector<SKeyValue> const& values, std::string_view path)
{
	auto const it{ std::ranges::find(values, path, &SKeyValue::path) };

	return (it != values.end()) ? std::string_view{ it->value } : std::string_view{};
}

//////////////////////////////////////////////////////////////////////////
bool IsFullyInstalled(std::string_view stateFlags)
{
	uint32_t flags{ 0 };
	std::from_chars_result const parsed{ std::from_chars(stateFlags.data(), stateFlags.data() + stateFlags.size(), flags) };

	return parsed.ec == std::errc{} && (flags & FullyInstalled) != 0;
}

//////////////////////////////////////////////////////////////////////////
// The manifest names a folder inside steamapps/common, so a name that would lead out of it is refused.
bool IsFolderName(std::string_view name)
{
	return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\:") == std::string_view::npos;
}

//////////////////////////////////////////////////////////////////////////
std::optional<SLaunchOption> MakeSteamOption(std::filesystem::path const& manifest, std::filesystem::path const& library,
	Query::SWindowsLaunchHints const& facts, std::string_view gameName)
{
	std::optional<SLaunchOption> option{};
	std::expected<std::vector<SKeyValue>, std::string> const read{ ReadSteamFile(manifest) };
	bool const isInstalled{ read.has_value() && IsFullyInstalled(FindValue(*read, "appstate/stateflags")) };
	std::string_view const installDir{ read.has_value() ? FindValue(*read, "appstate/installdir") : std::string_view{} };

	if (!read.has_value())
	{
		gLog.Warning("Cannot read Steam's manifest '{}': {}", manifest.string(), read.error());
	}
	else if (isInstalled && !IsFolderName(installDir))
	{
		gLog.Warning("Steam's manifest '{}' names no install folder Lookout can use: '{}'", manifest.string(), installDir);
	}
	else if (isInstalled)
	{
		std::filesystem::path const folder{ library / "steamapps" / "common" / installDir };
		std::optional<std::filesystem::path> const program{ FindInstallProgram(folder, facts.program, facts.requiredFiles, gameName) };
		std::string_view const name{ FindValue(*read, "appstate/name") };

		if (program.has_value())
		{
			option = SLaunchOption{ std::string{ SteamLauncherId }, std::string{ name.empty() ? gameName : name }, folder.string(),
				{ program->string() }, folder.string() };
		}
	}

	return option;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CSteamSource::Initialize(SLaunchEnvironment const& environment)
{
	m_pEnvironment = &environment;
}

//////////////////////////////////////////////////////////////////////////
void CSteamSource::Terminate()
{
	m_pEnvironment = nullptr;
}

//////////////////////////////////////////////////////////////////////////
// The first library holding the game's manifest is where Steam installed it.
std::vector<SLaunchOption> CSteamSource::Find(Query::SLaunchHints const& hints, std::string_view gameName) const
{
	Query::SWindowsLaunchHints const& facts{ hints.onWindows };
	std::vector<std::filesystem::path> const& libraries{ m_pEnvironment->steamLibraries };
	std::vector<SLaunchOption> options{};
	bool isFound{ false };

	for (size_t index{ 0 }; facts.steamAppId != 0 && !isFound && index < libraries.size(); ++index)
	{
		std::filesystem::path const manifest{ libraries[index] / "steamapps" / std::format("appmanifest_{}.acf", facts.steamAppId) };
		std::error_code error{};

		isFound = std::filesystem::exists(manifest, error);

		if (isFound)
		{
			std::optional<SLaunchOption> option{ MakeSteamOption(manifest, libraries[index], facts, gameName) };

			if (option.has_value())
			{
				options.emplace_back(std::move(*option));
			}
		}
		else if (error.value() != 0 && error != std::errc::no_such_file_or_directory)
		{
			gLog.Warning("Cannot look for Steam's manifest '{}': {}", manifest.string(), error.message());
		}
	}

	return options;
}
} // namespace Lkt::Launch
