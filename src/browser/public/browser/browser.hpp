#pragma once

#include "browser/auto_refresh.hpp"
#include "browser/game_status.hpp"
#include "browser/install_launcher.hpp"
#include "browser/launch_state.hpp"
#include "browser/server_entry.hpp"
#include "browser/server_list.hpp"
#include "config/install_kind.hpp"
#include "config/server_filter.hpp"
#include "config/settings.hpp"
#include "config/settings_store.hpp"
#include "config/sort_order.hpp"
#include "config/window_settings.hpp"
#include "launch/game_launcher.hpp"
#include "launch/launch_environment.hpp"
#include "launch/launch_error.hpp"
#include "launch/launch_option.hpp"
#include "net/clock.hpp"
#include "net/query_engine.hpp"
#include "query/game.hpp"
#include "query/game_definition.hpp"
#include "query/game_problem.hpp"
#include "query/parse_error.hpp"
#include "query/protocol_definition.hpp"
#include "query/server_address.hpp"
#include <tge/non_copyable.hpp>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Browser
{
// Spans from the getters stay valid until the next non-const call.
class CBrowser final : private Tge::SNoCopyNoMove
{
public:

	CBrowser() = default;
	~CBrowser() = default;

	void Initialize(std::string_view configDir, std::string_view logsDir, Launch::SLaunchEnvironment environment);
	bool Start(std::function<void()> onEventsReady);
	void Terminate();
	void Update();

	void SelectGame(Query::EGame game);
	void Refresh();
	void RefreshServer(Query::SServerAddress const& address);
	void ToggleFavourite(Query::SServerAddress const& address);
	// Takes the address a player joins and returns the one Lookout queries, which identifies the server.
	std::expected<Query::SServerAddress, Query::EParseError> AddServer(std::string_view text);
	std::expected<void, Launch::ELaunchError> Join(Query::SServerAddress const& joinAddress, std::string_view password, std::string_view launcherId);
	void SetFilter(Config::SServerFilter const& filter);
	void SetSort(Config::SSortOrder const& sort);
	void SetWindowSettings(Config::SWindowSettings const& window);
	void SetAutoRefresh(uint32_t seconds);
	void SetAutoRefreshPaused(bool isPaused);
	void AddInstall(Query::EGame game, Config::EInstallKind kind, std::string_view location);
	void SetInstallName(Query::EGame game, uint32_t id, std::string_view name);
	void SetInstallCommand(Query::EGame game, uint32_t id, std::string_view command);
	void SetInstallFolder(Query::EGame game, uint32_t id, std::string_view folder);
	void RemoveInstall(Query::EGame game, uint32_t id);
	void SetGameListed(Query::EGame game, bool isListed);
	void MoveGame(Query::EGame game, Query::EGame target);
	// False, with nothing touched, when the catalog is the one that runs.
	bool ReplaceCatalog(std::vector<Query::SProtocolDefinition> protocols, std::vector<Query::SGameDefinition> games);
	void SetGameProblems(std::vector<Query::SGameProblem> problems);

	Query::EGame GetSelectedGame() const;
	Config::SSettings const& GetSettings() const;
	std::span<SServerEntry const> GetEntries() const;
	std::span<uint32_t const> GetRows() const;
	std::span<std::string const> GetMods() const;
	std::span<uint8_t const> GetCountries() const;
	// None while off, paused or refreshing: the refresh's end wakes the window anyway.
	std::optional<Net::Clock::time_point> GetNextAutoRefresh() const;
	SServerEntry const* FindEntry(uint64_t key) const;
	SGameStatus const& GetStatus(Query::EGame game) const;
	std::span<Query::SGameProblem const> GetGameProblems() const;
	bool IsFavourite(Query::SServerAddress const& address) const;
	std::span<Launch::SLaunchOption const> GetLaunchOptions(Query::EGame game) const;
	std::span<SInstallLauncher const> GetInstallLaunchers(Query::EGame game) const;
	std::expected<Launch::SLaunchOption, Launch::ELaunchError> const& GetJoinLauncher(Query::EGame game) const;
	std::expected<Launch::SLaunchOption, Launch::ELaunchError> ResolveLauncher(Query::EGame game, std::string_view launcherId) const;

private:

	void SwapCatalog(std::vector<Query::SProtocolDefinition> protocols, std::vector<Query::SGameDefinition> games);
	void SetUpGame(Query::SGameDefinition const& game);
	SInstallLauncher ResolveInstall(Query::EGame game, Config::SGameInstall const& install) const;
	std::optional<size_t> FindInstallIndex(Query::EGame game, uint32_t id) const;
	void UpdateJoinLauncher(Query::EGame game);
	void Recount(Query::EGame game);
	void RebuildRows();
	bool IsGameSelected() const;
	size_t GetSelectedIndex() const;

	Config::CSettingsStore m_settingsStore;
	Config::SSettings m_settings;
	Launch::SLaunchEnvironment m_environment;
	Launch::CGameLauncher m_launcher;
	Net::CQueryEngine m_engine;
	std::function<void()> m_onEventsReady;
	std::vector<std::unique_ptr<CServerList>> m_lists;
	std::vector<SGameStatus> m_statuses;
	std::vector<SLaunchState> m_launchStates;
	std::vector<bool> m_hasChanged;
	std::vector<Net::SQueryEvent> m_events;
	std::vector<uint32_t> m_rows;
	std::vector<std::string> m_mods;
	std::vector<uint8_t> m_countries;
	std::vector<Query::SGameProblem> m_gameProblems;
	CAutoRefresh m_autoRefresh;
	bool m_isStarted{ false };
	bool m_isAutoRefreshPaused{ false };
};
} // namespace Lkt::Browser
