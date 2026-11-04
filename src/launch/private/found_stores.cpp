#include "launch/found_stores.hpp"
#include "program_path.hpp"
#include "steam_libraries.hpp"
#include <tge/platform.hpp>
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(TGE_PLATFORM_LINUX)
#include <expected>
#endif // defined(TGE_PLATFORM_LINUX)

namespace Lkt::Launch
{
namespace
{
#if defined(TGE_PLATFORM_LINUX)
constexpr std::string_view HomePlaceholder{ "{home}/" };
constexpr std::string_view DataHomePlaceholder{ "{dataHome}/" };
#endif // defined(TGE_PLATFORM_LINUX)

//////////////////////////////////////////////////////////////////////////
std::vector<std::filesystem::path> ReadLibraryFolders(Query::EStoreLibraries libraries, std::filesystem::path const& folder)
{
	std::vector<std::filesystem::path> folders{};

	switch (libraries)
	{
		case Query::EStoreLibraries::Steam:
			folders = ReadSteamLibraries(folder);
			break;
	}

	return folders;
}

//////////////////////////////////////////////////////////////////////////
// Two ways to the same install, such as Steam's two registry values, count once.
bool IsFound(std::vector<SFoundStore> const& found, std::string_view key, std::filesystem::path const& folder)
{
	return std::ranges::any_of(found, [key, &folder](SFoundStore const& store)
	{
		std::error_code error{};

		return store.key == key && std::filesystem::equivalent(store.folder, folder, error) && error.value() == 0;
	});
}

//////////////////////////////////////////////////////////////////////////
// Without a program the store still counts, so a game its libraries hold can ask the user where the program is.
void AddStore(Query::SStoreDefinition const& store, std::filesystem::path const& folder, std::filesystem::path const& program, std::vector<SFoundStore>& found)
{
	if (!IsFound(found, store.key, folder))
	{
		found.emplace_back(SFoundStore{ store.key, store.name, store.arguments, store.libraries, program, ReadLibraryFolders(store.libraries, folder), folder });
	}
}

//////////////////////////////////////////////////////////////////////////
// One the user located, however the store file would look for it; nothing when it no longer runs.
std::optional<std::filesystem::path> FindLocatedProgram(std::span<SLocatedProgram const> located, std::string_view key)
{
	auto const it{ std::ranges::find(located, key, &SLocatedProgram::key) };

	return (it != located.end()) ? std::optional<std::filesystem::path>{ CheckExecutable(it->program).has_value() ? it->program : std::filesystem::path{} }
		: std::nullopt;
}

#if defined(TGE_PLATFORM_WINDOWS)
//////////////////////////////////////////////////////////////////////////
void FindOnWindows(Query::SStoreDefinition const& store, SLaunchEnvironment const& environment, std::span<SLocatedProgram const> located,
	std::vector<SFoundStore>& found)
{
	std::optional<std::filesystem::path> const locatedProgram{ FindLocatedProgram(located, store.key) };

	for (Query::SWindowsStoreInstall const& install : store.onWindows)
	{
		// No reader, no registry: a test's environment must never find, and start, the machine's real store.
		std::filesystem::path folder{ (environment.readStoreFolder != nullptr) ? environment.readStoreFolder(install.folder) : std::filesystem::path{} };
		std::error_code error{};

		folder.make_preferred();

		if (!folder.empty() && std::filesystem::is_directory(folder, error))
		{
			bool const hasProgram{ CheckExecutable(folder / install.program).has_value() };

			AddStore(store, folder, locatedProgram.value_or(hasProgram ? folder / install.program : std::filesystem::path{}), found);
		}
	}
}
#elif defined(TGE_PLATFORM_LINUX)
//////////////////////////////////////////////////////////////////////////
// Nothing for a placeholder the environment cannot fill.
std::optional<std::filesystem::path> ExpandStorePath(std::string_view path, SLaunchEnvironment const& environment)
{
	std::optional<std::filesystem::path> expanded{};

	if (path.starts_with(HomePlaceholder) && !environment.home.empty())
	{
		expanded = environment.home / path.substr(HomePlaceholder.size());
	}
	else if (path.starts_with(DataHomePlaceholder) && !environment.dataHome.empty())
	{
		expanded = environment.dataHome / path.substr(DataHomePlaceholder.size());
	}
	else if (path.starts_with('/'))
	{
		expanded = std::filesystem::path{ path };
	}

	return expanded;
}

//////////////////////////////////////////////////////////////////////////
// The first that runs: a name is looked for on PATH, as the shell would.
std::optional<std::filesystem::path> FindStoreProgram(std::vector<std::string> const& programs, SLaunchEnvironment const& environment)
{
	std::optional<std::filesystem::path> found{};

	for (size_t index{ 0 }; !found.has_value() && index < programs.size(); ++index)
	{
		std::string_view const program{ programs[index] };

		if (!program.contains('/'))
		{
			std::expected<std::filesystem::path, EProgramError> const resolved{ ResolveProgram(program, {}, environment.searchPath) };

			found = resolved.has_value() ? std::optional<std::filesystem::path>{ *resolved } : std::nullopt;
		}
		else if (std::optional<std::filesystem::path> const path{ ExpandStorePath(program, environment) }; path.has_value() && CheckExecutable(*path).has_value())
		{
			found = *path;
		}
	}

	return found;
}

//////////////////////////////////////////////////////////////////////////
void FindOnLinux(Query::SStoreDefinition const& store, SLaunchEnvironment const& environment, std::span<SLocatedProgram const> located,
	std::vector<SFoundStore>& found)
{
	std::optional<std::filesystem::path> const locatedProgram{ FindLocatedProgram(located, store.key) };

	for (Query::SLinuxStoreInstall const& install : store.onLinux)
	{
		std::optional<std::filesystem::path> const folder{ ExpandStorePath(install.folder, environment) };
		std::error_code error{};

		if (folder.has_value() && std::filesystem::is_directory(*folder, error))
		{
			AddStore(store, *folder, locatedProgram.has_value() ? *locatedProgram : FindStoreProgram(install.programs, environment).value_or(std::filesystem::path{}),
				found);
		}
	}
}
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
} // namespace

//////////////////////////////////////////////////////////////////////////
// Each install of a store pairs a folder with its own program, so a game only that install holds starts through it.
std::vector<SFoundStore> FindStores(std::span<Query::SStoreDefinition const> stores, SLaunchEnvironment const& environment,
	std::span<SLocatedProgram const> located)
{
	std::vector<SFoundStore> found{};

	for (Query::SStoreDefinition const& store : stores)
	{
#if defined(TGE_PLATFORM_WINDOWS)
		FindOnWindows(store, environment, located, found);
#elif defined(TGE_PLATFORM_LINUX)
		FindOnLinux(store, environment, located, found);
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
	}

	return found;
}
} // namespace Lkt::Launch
