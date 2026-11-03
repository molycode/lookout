#include "store_source.hpp"
#include "steam_apps.hpp"
#include "launch/launcher_ids.hpp"
#include "query/store_ids.hpp"
#include <algorithm>
#include <format>
#include <optional>
#include <string>

namespace Lkt::Launch
{
namespace
{
constexpr std::string_view IdPlaceholder{ "{id}" };

//////////////////////////////////////////////////////////////////////////
// The store's own name for the game, or nothing when the game is not installed in it.
std::optional<std::string> FindInstalledName(SFoundStore const& store, std::string_view id)
{
	std::optional<std::string> name{};

	switch (store.libraries)
	{
		case Query::EStoreLibraries::Steam:
			if (std::optional<SSteamApp> const app{ FindInstalledSteamApp(store.libraryFolders, id) }; app.has_value())
			{
				name = app->name;
			}

			break;
	}

	return name;
}

//////////////////////////////////////////////////////////////////////////
std::string FillId(std::string argument, std::string_view id)
{
	for (size_t at{ argument.find(IdPlaceholder) }; at != std::string::npos; at = argument.find(IdPlaceholder, at + id.size()))
	{
		argument.replace(at, IdPlaceholder.size(), id);
	}

	return argument;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CStoreSource::Initialize(std::span<SFoundStore const> stores)
{
	m_stores = stores;
}

//////////////////////////////////////////////////////////////////////////
void CStoreSource::Terminate()
{
	m_stores = {};
}

//////////////////////////////////////////////////////////////////////////
// The store sets the game's working folder itself. A store found twice offers the game once, through the first.
std::vector<SLaunchOption> CStoreSource::Find(Query::SLaunchHints const& hints, std::string_view gameName) const
{
	std::vector<SLaunchOption> options{};

	for (SFoundStore const& store : m_stores)
	{
		std::string_view const id{ Query::FindStoreId(hints, store.key) };
		std::string const launcherId{ std::format("{}{}", StoreLauncherIdPrefix, store.key) };
		bool const isListed{ std::ranges::contains(options, launcherId, &SLaunchOption::id) };
		std::optional<std::string> const installedName{ (!id.empty() && !isListed) ? FindInstalledName(store, id) : std::nullopt };

		if (installedName.has_value())
		{
			std::vector<std::string> argv{ store.program.string() };

			for (std::string const& argument : store.arguments)
			{
				argv.emplace_back(FillId(argument, id));
			}

			options.emplace_back(SLaunchOption{ launcherId, installedName->empty() ? std::string{ gameName } : *installedName,
				std::format("through {}", store.name), std::move(argv), {} });
		}
	}

	return options;
}
} // namespace Lkt::Launch
