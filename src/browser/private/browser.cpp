#include "browser/browser.hpp"
#include "launcher_choice.hpp"
#include "loggers.hpp"
#include "server_countries.hpp"
#include "server_mods.hpp"
#include "server_rows.hpp"
#include "config/first_listed_game.hpp"
#include "config/install_ids.hpp"
#include "launch/command_option.hpp"
#include "launch/connect_request.hpp"
#include "launch/folder_option.hpp"
#include "launch/home_path.hpp"
#include "launch/launch_discovery.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include "query/join_address.hpp"
#include <tge/assert.hpp>
#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace Lkt::Browser
{
namespace
{
constexpr std::string_view Blanks{ " \t\r\n" };

//////////////////////////////////////////////////////////////////////////
std::string_view Trim(std::string_view text)
{
	size_t const start{ text.find_first_not_of(Blanks) };
	size_t const end{ text.find_last_not_of(Blanks) };

	return (start != std::string_view::npos) ? text.substr(start, end - start + 1) : std::string_view{};
}

//////////////////////////////////////////////////////////////////////////
size_t ToIndex(Query::EGame game)
{
	return static_cast<size_t>(game);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CBrowser::Initialize(std::string_view configDir, std::string_view logsDir, Launch::SLaunchEnvironment environment)
{
	size_t const numGames{ Query::GetGameCatalog().size() };

	m_lists = std::vector<CServerList>(numGames);
	m_statuses.assign(numGames, SGameStatus{});
	m_launchStates.assign(numGames, SLaunchState{});
	m_hasChanged.assign(numGames, false);
	m_autoRefresh.Initialize(numGames);
	m_environment = std::move(environment);
	m_settingsStore.Initialize(configDir);
	m_settings = m_settingsStore.Load();
	m_launcher.Initialize(logsDir);

	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		for (Query::SServerAddress const& favourite : m_settings.games[ToIndex(game.game)].favourites)
		{
			m_lists[ToIndex(game.game)].SetFavourite(game, favourite, true);
		}

		SLaunchState& state{ m_launchStates[ToIndex(game.game)] };

		state.options = Launch::FindLaunchOptions(game, m_environment);
		state.installs.clear();

		for (Config::SGameInstall const& install : m_settings.games[ToIndex(game.game)].installs)
		{
			state.installs.emplace_back(ResolveInstall(game.game, install));
		}

		UpdateJoinLauncher(game.game);
		Recount(game.game);
	}

	RebuildRows();
}

//////////////////////////////////////////////////////////////////////////
bool CBrowser::Start(std::function<void()> onEventsReady)
{
	m_isStarted = m_engine.Initialize(std::move(onEventsReady));

	return m_isStarted;
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::Terminate()
{
	m_engine.Terminate();
	m_isStarted = false;
	m_settingsStore.Save(m_settings);
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::Update()
{
	std::ranges::fill(m_hasChanged, false);

	if (m_isStarted)
	{
		m_engine.TakeEvents(m_events);
	}

	for (Net::SQueryEvent& event : m_events)
	{
		Query::EGame const game{ std::visit([](auto const& typed) { return typed.game; }, event) };
		size_t const index{ ToIndex(game) };

		if (m_lists[index].Apply(Query::GetGame(game), std::move(event)))
		{
			m_statuses[index].isRefreshing = false;
		}

		m_hasChanged[index] = true;
	}

	m_events.clear();

	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		if (m_hasChanged[ToIndex(game.game)])
		{
			Recount(game.game);
		}
	}

	if (m_hasChanged[GetSelectedIndex()])
	{
		CollectMods(GetEntries(), m_mods);
		CollectCountries(GetEntries(), m_countries);
		RebuildRows();
	}

	std::optional<Net::Clock::time_point> const nextAutoRefresh{ GetNextAutoRefresh() };

	if (m_isStarted && nextAutoRefresh.has_value() && Net::Clock::now() >= *nextAutoRefresh)
	{
		Refresh();
	}

	m_launcher.ReapFinished();
}

//////////////////////////////////////////////////////////////////////////
// A game is refreshed on first sight only: after that, refreshing is the user's call.
void CBrowser::SelectGame(Query::EGame game)
{
	m_settings.selectedGame = game;

	if (!m_statuses[ToIndex(game)].hasRefreshed)
	{
		Refresh();
	}

	CollectMods(GetEntries(), m_mods);
	CollectCountries(GetEntries(), m_countries);
	RebuildRows();
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::Refresh()
{
	size_t const index{ GetSelectedIndex() };
	uint32_t const refreshId{ m_engine.Refresh(m_settings.selectedGame, m_settings.games[index].favourites) };

	m_autoRefresh.OnRefreshStarted(m_settings.selectedGame, Net::Clock::now());
	m_lists[index].BeginRefresh(refreshId);
	m_statuses[index].hasRefreshed = true;
	m_statuses[index].isRefreshing = true;
	Recount(m_settings.selectedGame);
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::RefreshServer(Query::SServerAddress const& address)
{
	m_engine.RefreshServer(m_settings.selectedGame, address);
	m_statuses[GetSelectedIndex()].isRefreshing = true;
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::ToggleFavourite(Query::SServerAddress const& address)
{
	size_t const index{ GetSelectedIndex() };
	std::vector<Query::SServerAddress>& favourites{ m_settings.games[index].favourites };
	auto const it{ std::ranges::find(favourites, address) };
	bool const isFavourite{ it == favourites.end() };

	if (isFavourite)
	{
		favourites.emplace_back(address);
	}
	else
	{
		favourites.erase(it);
	}

	m_lists[index].SetFavourite(Query::GetGame(m_settings.selectedGame), address, isFavourite);
	m_settingsStore.Save(m_settings);
	Recount(m_settings.selectedGame);
	RebuildRows();
}

//////////////////////////////////////////////////////////////////////////
// No IsQueryable check: a server added by hand may well be on the LAN.
std::expected<Query::SServerAddress, Query::EParseError> CBrowser::AddServer(std::string_view text)
{
	Query::SGameDefinition const& game{ Query::GetGame(m_settings.selectedGame) };
	std::string_view const trimmed{ Trim(text) };
	std::expected<Query::SServerAddress, Query::EParseError> const joinAddress{ Query::ParseAddress(trimmed) };
	std::expected<Query::SServerAddress, Query::EParseError> const address{ joinAddress.and_then([&game](Query::SServerAddress const& typed)
	{
		return Query::ToQueryAddress(game, typed);
	}) };

	if (address.has_value())
	{
		size_t const index{ GetSelectedIndex() };
		std::vector<Query::SServerAddress>& favourites{ m_settings.games[index].favourites };

		if (!std::ranges::contains(favourites, *address))
		{
			favourites.emplace_back(*address);
			m_lists[index].SetFavourite(game, *address, true);
			m_settingsStore.Save(m_settings);
		}

		RefreshServer(*address);
		Recount(m_settings.selectedGame);
		RebuildRows();
	}
	else if (joinAddress.has_value())
	{
		gLog.Warning("'{}' cannot be a {} server: its query port, {}{:+}, is out of range", trimmed, game.name, joinAddress->port, game.queryPortOffset);
	}
	else
	{
		gLog.Warning("'{}' is not a server address: expected a.b.c.d:port", trimmed);
	}

	return address;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, Launch::ELaunchError> CBrowser::Join(Query::SServerAddress const& joinAddress, std::string_view password, std::string_view launcherId)
{
	Query::SGameDefinition const& game{ Query::GetGame(m_settings.selectedGame) };
	std::expected<Launch::SLaunchOption, Launch::ELaunchError> const choice{ ResolveLauncher(m_settings.selectedGame, launcherId) };
	std::expected<void, Launch::ELaunchError> result{};

	if (choice.has_value())
	{
		result = m_launcher.Launch(game, *choice, Launch::SConnectRequest{ joinAddress, std::string{ password } });
	}
	else
	{
		gLog.Warning("Cannot join {} with {}: {}", Query::FormatAddress(joinAddress), game.name, Launch::ToString(choice.error()));
		result = std::unexpected{ choice.error() };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::SetFilter(Config::SServerFilter const& filter)
{
	m_settings.games[GetSelectedIndex()].filter = filter;
	RebuildRows();
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::SetSort(Config::SSortOrder const& sort)
{
	m_settings.games[GetSelectedIndex()].sort = sort;
	RebuildRows();
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::SetWindowSettings(Config::SWindowSettings const& window)
{
	m_settings.window = window;
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::SetAutoRefresh(uint32_t seconds)
{
	m_settings.autoRefreshSeconds = seconds;
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::SetAutoRefreshPaused(bool isPaused)
{
	m_isAutoRefreshPaused = isPaused;
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::AddInstall(Query::EGame game, Config::EInstallKind kind, std::string_view location)
{
	Config::SGameSettings& settings{ m_settings.games[ToIndex(game)] };
	uint32_t const id{ Config::NextInstallId(settings.installs) };

	settings.installs.emplace_back(Config::SGameInstall{ id, {}, kind, std::string{ location } });
	m_launchStates[ToIndex(game)].installs.emplace_back(ResolveInstall(game, settings.installs.back()));
	UpdateJoinLauncher(game);
	m_settingsStore.Save(m_settings);
}

//////////////////////////////////////////////////////////////////////////
// Saved with the rest, as the filter is: it changes with every key typed.
void CBrowser::SetInstallName(Query::EGame game, uint32_t id, std::string_view name)
{
	std::optional<size_t> const index{ FindInstallIndex(game, id) };

	if (index.has_value())
	{
		Config::SGameInstall& install{ m_settings.games[ToIndex(game)].installs[*index] };
		SInstallLauncher& launcher{ m_launchStates[ToIndex(game)].installs[*index] };

		install.name = name;
		launcher.name = install.name.empty() ? std::string{ Query::GetGame(game).name } : install.name;

		if (launcher.option.has_value())
		{
			launcher.option->name = launcher.name;
		}

		UpdateJoinLauncher(game);
	}
}

//////////////////////////////////////////////////////////////////////////
// Saved with the rest, as the filter is: it changes with every key typed.
void CBrowser::SetInstallCommand(Query::EGame game, uint32_t id, std::string_view command)
{
	std::optional<size_t> const index{ FindInstallIndex(game, id) };

	if (index.has_value())
	{
		Config::SGameInstall& install{ m_settings.games[ToIndex(game)].installs[*index] };

		TGE_ASSERT(install.kind == Config::EInstallKind::Command, "A folder install is given a command");

		install.location = command;
		m_launchStates[ToIndex(game)].installs[*index] = ResolveInstall(game, install);
		UpdateJoinLauncher(game);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::SetInstallFolder(Query::EGame game, uint32_t id, std::string_view folder)
{
	std::optional<size_t> const index{ FindInstallIndex(game, id) };

	if (index.has_value())
	{
		Config::SGameInstall& install{ m_settings.games[ToIndex(game)].installs[*index] };

		TGE_ASSERT(install.kind == Config::EInstallKind::Folder, "A command install is given a folder");

		install.location = folder;
		m_launchStates[ToIndex(game)].installs[*index] = ResolveInstall(game, install);
		UpdateJoinLauncher(game);
		m_settingsStore.Save(m_settings);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::RemoveInstall(Query::EGame game, uint32_t id)
{
	std::optional<size_t> const index{ FindInstallIndex(game, id) };

	if (index.has_value())
	{
		Config::SGameSettings& settings{ m_settings.games[ToIndex(game)] };
		std::vector<SInstallLauncher>& launchers{ m_launchStates[ToIndex(game)].installs };

		settings.installs.erase(settings.installs.begin() + static_cast<std::ptrdiff_t>(*index));
		launchers.erase(launchers.begin() + static_cast<std::ptrdiff_t>(*index));
		UpdateJoinLauncher(game);
		m_settingsStore.Save(m_settings);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::SetGameListed(Query::EGame game, bool isListed)
{
	Config::SGameSettings& settings{ m_settings.games[ToIndex(game)] };

	settings.isListed = isListed;

	std::optional<Query::EGame> const firstListed{ Config::FindFirstListedGame(m_settings) };

	if (firstListed.has_value())
	{
		if (!isListed && game == m_settings.selectedGame)
		{
			SelectGame(*firstListed);
		}

		m_settingsStore.Save(m_settings);
	}
	else
	{
		settings.isListed = true;
		gLog.Warning("{} stays in the sidebar, since no other game is there", Query::GetGame(game).name);
	}
}

//////////////////////////////////////////////////////////////////////////
Query::EGame CBrowser::GetSelectedGame() const
{
	return m_settings.selectedGame;
}

//////////////////////////////////////////////////////////////////////////
Config::SSettings const& CBrowser::GetSettings() const
{
	return m_settings;
}

//////////////////////////////////////////////////////////////////////////
std::span<SServerEntry const> CBrowser::GetEntries() const
{
	return m_lists[GetSelectedIndex()].GetEntries();
}

//////////////////////////////////////////////////////////////////////////
std::span<uint32_t const> CBrowser::GetRows() const
{
	return m_rows;
}

//////////////////////////////////////////////////////////////////////////
std::span<std::string const> CBrowser::GetMods() const
{
	return m_mods;
}

//////////////////////////////////////////////////////////////////////////
std::span<uint8_t const> CBrowser::GetCountries() const
{
	return m_countries;
}

//////////////////////////////////////////////////////////////////////////
std::optional<Net::Clock::time_point> CBrowser::GetNextAutoRefresh() const
{
	std::optional<Net::Clock::time_point> deadline{};

	if (!m_isAutoRefreshPaused && !m_statuses[GetSelectedIndex()].isRefreshing)
	{
		deadline = m_autoRefresh.GetDeadline(m_settings.selectedGame, std::chrono::seconds{ m_settings.autoRefreshSeconds });
	}

	return deadline;
}

//////////////////////////////////////////////////////////////////////////
SServerEntry const* CBrowser::FindEntry(uint64_t key) const
{
	return m_lists[GetSelectedIndex()].Find(Query::FromKey(key));
}

//////////////////////////////////////////////////////////////////////////
SGameStatus const& CBrowser::GetStatus(Query::EGame game) const
{
	return m_statuses[ToIndex(game)];
}

//////////////////////////////////////////////////////////////////////////
bool CBrowser::IsFavourite(Query::SServerAddress const& address) const
{
	SServerEntry const* const pEntry{ m_lists[GetSelectedIndex()].Find(address) };

	return pEntry != nullptr && pEntry->isFavourite;
}

//////////////////////////////////////////////////////////////////////////
std::span<Launch::SLaunchOption const> CBrowser::GetLaunchOptions(Query::EGame game) const
{
	return m_launchStates[ToIndex(game)].options;
}

//////////////////////////////////////////////////////////////////////////
std::span<SInstallLauncher const> CBrowser::GetInstallLaunchers(Query::EGame game) const
{
	return m_launchStates[ToIndex(game)].installs;
}

//////////////////////////////////////////////////////////////////////////
std::expected<Launch::SLaunchOption, Launch::ELaunchError> const& CBrowser::GetJoinLauncher(Query::EGame game) const
{
	return m_launchStates[ToIndex(game)].joinLauncher;
}

//////////////////////////////////////////////////////////////////////////
std::expected<Launch::SLaunchOption, Launch::ELaunchError> CBrowser::ResolveLauncher(Query::EGame game, std::string_view launcherId) const
{
	SLaunchState const& state{ m_launchStates[ToIndex(game)] };

	return ChooseLauncher(state.options, state.installs, launcherId);
}

//////////////////////////////////////////////////////////////////////////
SInstallLauncher CBrowser::ResolveInstall(Query::EGame game, Config::SGameInstall const& install) const
{
	bool const isFolder{ install.kind == Config::EInstallKind::Folder };
	SInstallLauncher launcher{ Config::ToLauncherId(install.id), install.name.empty() ? std::string{ Query::GetGame(game).name } : install.name,
		isFolder ? Launch::ShortenHome(install.location, m_environment.home) : install.location,
		isFolder ? Launch::MakeFolderOption(Query::GetGame(game), install.location) : Launch::MakeCommandOption(install.location) };

	if (launcher.option.has_value())
	{
		launcher.option->id = launcher.id;
		launcher.option->name = launcher.name;
		launcher.option->location = launcher.location;
	}

	return launcher;
}

//////////////////////////////////////////////////////////////////////////
std::optional<size_t> CBrowser::FindInstallIndex(Query::EGame game, uint32_t id) const
{
	std::vector<Config::SGameInstall> const& installs{ m_settings.games[ToIndex(game)].installs };
	auto const it{ std::ranges::find(installs, id, &Config::SGameInstall::id) };

	TGE_ASSERT(it != installs.end(), "An install is changed by an id the game does not have");

	return (it != installs.end()) ? std::optional<size_t>{ static_cast<size_t>(it - installs.begin()) } : std::nullopt;
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::UpdateJoinLauncher(Query::EGame game)
{
	SLaunchState& state{ m_launchStates[ToIndex(game)] };

	state.joinLauncher = ChooseLauncher(state.options, state.installs, {});
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::Recount(Query::EGame game)
{
	size_t const index{ ToIndex(game) };
	CServerList const& list{ m_lists[index] };
	SGameStatus& status{ m_statuses[index] };
	std::span<SServerEntry const> const entries{ list.GetEntries() };

	status.numListed = static_cast<uint32_t>(entries.size());
	status.numAnswered = list.GetNumAnswered();
	status.numMastersFailed = list.GetNumMastersFailed();
	status.numPlayers = 0;

	for (SServerEntry const& entry : entries)
	{
		if (entry.state == EServerState::Online)
		{
			status.numPlayers += entry.summary.numPlayers;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrowser::RebuildRows()
{
	Config::SGameSettings const& settings{ m_settings.games[GetSelectedIndex()] };

	BuildRows(GetEntries(), settings.filter, settings.sort, m_rows);
}

//////////////////////////////////////////////////////////////////////////
size_t CBrowser::GetSelectedIndex() const
{
	return ToIndex(m_settings.selectedGame);
}
} // namespace Lkt::Browser
