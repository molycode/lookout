#include "cli/session.hpp"
#include "app_folder.hpp"
#include "run_request.hpp"
#include "game_keys.hpp"
#include "loggers.hpp"
#include "parse_command_line.hpp"
#include "run_context.hpp"
#include "browser/browser.hpp"
#include "browser/server_list.hpp"
#include "config/version_check.hpp"
#include "download/game_downloads.hpp"
#include "download/latest_release.hpp"
#include "download/lookout_games.hpp"
#include "download/lookout_release.hpp"
#include "games/check_folder.hpp"
#include "games/game_files.hpp"
#include "games/load_games.hpp"
#include "net/query_engine.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include "query/store_catalog.hpp"
#include <tge/assert.hpp>
#include <tge/logging/log_system.hpp>
#include <tge/module/runtime.hpp>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <expected>
#include <format>
#include <mutex>
#include <print>
#include <system_error>
#include <utility>

namespace Lkt::Cli
{
namespace
{
constexpr size_t MaxLogFiles{ 10 };

// tge's runtime names every log file "tge_<date>_<time>.log", so the name orders them by age.
constexpr std::string_view LogFilePrefix{ "tge_" };
constexpr std::string_view LogFileExtension{ ".log" };

constexpr std::chrono::milliseconds ListUpdateInterval{ 250 };

//////////////////////////////////////////////////////////////////////////
std::vector<std::string_view> GetGameKeys()
{
	std::vector<std::string_view> keys{};

	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		keys.emplace_back(game.key);
	}

	return keys;
}

//////////////////////////////////////////////////////////////////////////
void PrintUsage(std::string_view executable, std::span<std::string_view const> gameKeys)
{
	std::println("Usage: {} [options]", executable);
	std::println("Options:");
	std::println("  --list <game>         Print the game's servers and exit; <game> is one of: {}", FormatGameKeys(gameKeys));
	std::println("  --download [game...]  Download these games from lookout-games, or each one missing or with an update, and exit");
	std::println("  --check <folder>      Check the games and protocols of a lookout-games folder, print each problem and exit");
	std::println("  --version             Show the version");
	std::println("  --help                Show this help message");
}

//////////////////////////////////////////////////////////////////////////
// Everything on stdout, so a pull request's check log reads in order.
int CheckFolder(std::filesystem::path const& folder)
{
	Games::SGameContent const content{ Games::CheckGameFolder(folder) };
	size_t const numProblems{ content.problems.size() };

	for (Query::SGameProblem const& problem : content.problems)
	{
		std::println("{}", problem.text);
	}

	std::println("{} games, {} protocols, {} stores, {} {}", content.games.size(), content.protocols.size(), content.stores.size(),
		(numProblems == 0) ? std::string{ "no" } : std::to_string(numProblems), (numProblems == 1) ? "problem" : "problems");

	return (numProblems == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//////////////////////////////////////////////////////////////////////////
// The same columns as tools/query.py, sorted the same way, so the two can be compared.
void PrintServers(Browser::CServerList const& servers)
{
	std::vector<Browser::SServerEntry const*> online{};

	for (Browser::SServerEntry const& entry : servers.GetEntries())
	{
		if (entry.state == Browser::EServerState::Online)
		{
			online.emplace_back(&entry);
		}
	}

	std::ranges::sort(online, [](Browser::SServerEntry const* pLhs, Browser::SServerEntry const* pRhs)
	{
		return Query::ToKey(pLhs->address) < Query::ToKey(pRhs->address);
	});

	for (Browser::SServerEntry const* pEntry : online)
	{
		Query::SServerSummary const& summary{ pEntry->summary };

		std::println("{} {}/{} {} {}", Query::FormatAddress(pEntry->joinAddress), summary.numPlayers, summary.maxPlayers, summary.map, summary.name.plain);
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// Problems are kept, not logged: logging reaches its file only once the runtime starts.
void CSession::Initialize()
{
	m_cryptoThreading.Initialize();

	std::expected<void, std::error_code> const sockets{ m_socketLibrary.Initialize() };

	if (!sockets.has_value())
	{
		m_startupProblems.emplace_back(std::format("Cannot start the socket library, so no server can be reached: {}", sockets.error().message()));
	}

	std::expected<std::filesystem::path, std::error_code> const dataDir{ GetAppFolder(Tge::IO::EUserFolder::Data) };

	m_userDir = dataDir.value_or(std::filesystem::path{});

	Games::SGameContent content{ Games::LoadGames(Games::GetDownloadedDir(m_userDir), m_userDir) };

	if (!dataDir.has_value())
	{
		content.problems.emplace_back(Query::SGameProblem{ std::format("Cannot locate the data directory, so no games can be loaded: {}",
			dataDir.error().message()), {} });
	}

	Query::InitializeGameCatalog(content.protocols, content.games);
	Query::InitializeStoreCatalog(content.stores);
	m_gameProblems = std::move(content.problems);
}

//////////////////////////////////////////////////////////////////////////
void CSession::Terminate()
{
	Query::TerminateStoreCatalog();
	Query::TerminateGameCatalog();
	m_socketLibrary.Terminate();
	m_cryptoThreading.Terminate();
}

//////////////////////////////////////////////////////////////////////////
// Only a command line without options opens the window, so a caller that always has options may pass no window.
int CSession::Run(std::span<char* const> args, std::function<bool(CSession&)> const& window)
{
	std::string_view const executable{ args.empty() ? "lookout" : args.front() };
	std::span<char* const> const optionArgs{ args.empty() ? args : args.subspan(1) };
	std::vector<std::string_view> const options{ optionArgs.begin(), optionArgs.end() };
	std::vector<std::string_view> const gameKeys{ GetGameKeys() };
	std::expected<SRunRequest, std::string> const request{ ParseCommandLine(options, gameKeys) };
	int result{ EXIT_FAILURE };

	if (!request.has_value())
	{
		std::println(stderr, "{}", request.error());
	}
	else if (request->command == ECommand::Help)
	{
		PrintUsage(executable, gameKeys);
		result = EXIT_SUCCESS;
	}
	else if (request->command == ECommand::Version)
	{
		std::println("Lookout {}", LKT_VERSION);
		result = EXIT_SUCCESS;
	}
	else if (request->command == ECommand::Check)
	{
		result = CheckFolder(request->checkFolder);
	}
	else
	{
		result = RunInRuntime(*request, window) ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
bool CSession::RunWindow(std::function<bool(CSession&)> const& window)
{
	return RunInRuntime(SRunRequest{}, window);
}

//////////////////////////////////////////////////////////////////////////
// A save in the editor reloads at once and again when the watcher sees it; the second has nothing new to say.
void CSession::ReloadGames(Browser::CBrowser& browser)
{
	Games::SGameContent content{ Games::LoadGames(Games::GetDownloadedDir(m_userDir), m_userDir) };
	size_t const numGames{ content.games.size() };
	bool const hasNewProblems{ !std::ranges::equal(content.problems, browser.GetGameProblems()) };
	bool const isReplaced{ browser.ReplaceCatalog(std::move(content.protocols), std::move(content.stores), std::move(content.games)) };

	if (isReplaced || hasNewProblems)
	{
		ReportGameProblems(content.problems);
		gLog.Info("Reloaded the game descriptions: {} {}", numGames, (numGames == 1) ? "game" : "games");
	}

	browser.SetGameProblems(std::move(content.problems));
}

//////////////////////////////////////////////////////////////////////////
std::string_view CSession::GetConfigDir() const
{
	return m_configDir;
}

//////////////////////////////////////////////////////////////////////////
std::string_view CSession::GetLogsDir() const
{
	return m_logsDir;
}

//////////////////////////////////////////////////////////////////////////
std::filesystem::path const& CSession::GetUserDir() const
{
	return m_userDir;
}

//////////////////////////////////////////////////////////////////////////
std::filesystem::path const& CSession::GetCacheDir() const
{
	return m_cacheDir;
}

//////////////////////////////////////////////////////////////////////////
std::vector<Query::SGameProblem> const& CSession::GetGameProblems() const
{
	return m_gameProblems;
}

//////////////////////////////////////////////////////////////////////////
bool CSession::RunInRuntime(SRunRequest const& request, std::function<bool(CSession&)> const& window)
{
	PrepareDirectories();

	bool const isListing{ request.command != ECommand::Window };

	if (isListing)
	{
		StartListLogging();
	}

	bool success{ false };
	bool const isRuntimeReady{ Tge::gRuntime->Initialize(MakeRunContext(m_logsDir, m_configDir)) };

	ReportStartupProblems();
	ReportGameProblems(m_gameProblems);

	if (isRuntimeReady)
	{
		gLog.Info("Lookout {} started", LKT_VERSION);
		PruneLogs();

		if (request.command == ECommand::Download)
		{
			success = RunDownload(request.downloadKeys);
		}
		else if (request.command == ECommand::List)
		{
			Query::SGameDefinition const* const pGame{ Query::FindGame(request.listGame) };

			TGE_ASSERT(pGame != nullptr, "The command line names only games of the catalog");
			success = RunList(*pGame);
		}
		else
		{
			TGE_ASSERT(window != nullptr, "Only a caller with a window runs without options");
			success = window(*this);
		}

		gLog.Info("Lookout terminated");
	}
	else
	{
		gLog.Error("The tge runtime failed to initialize");
	}

	if (isListing)
	{
		StopListLogging();
	}

	Tge::gRuntime->Terminate();

	return success;
}

//////////////////////////////////////////////////////////////////////////
bool CSession::RunList(Query::SGameDefinition const& game)
{
	Net::CQueryEngine engine{};
	std::mutex mutex{};
	std::condition_variable wake{};
	bool hasEvents{ false };
	bool success{ false };

	bool const isEngineReady{ engine.Initialize(Net::SQueryCatalog{ Query::GetGameCatalog(), Query::GetProtocolCatalog() }, [&mutex, &wake, &hasEvents]()
	{
		{
			std::lock_guard const lock{ mutex };

			hasEvents = true;
		}

		wake.notify_one();
	}) };

	if (isEngineReady)
	{
		Browser::CServerList servers{};
		std::vector<Net::SQueryEvent> events{};
		bool isFinished{ false };
		auto lastUpdate{ std::chrono::steady_clock::now() };

		servers.BeginRefresh(engine.Refresh(game.game, {}));

		while (!isFinished)
		{
			{
				std::unique_lock lock{ mutex };

				wake.wait_for(lock, ListUpdateInterval, [&hasEvents]() { return hasEvents; });
				hasEvents = false;
			}

			engine.TakeEvents(events);

			for (Net::SQueryEvent& event : events)
			{
				isFinished = servers.Apply(game, std::move(event)) || isFinished;
			}

			events.clear();

			auto const now{ std::chrono::steady_clock::now() };

			Tge::gRuntime->Update(std::chrono::duration<float>{ now - lastUpdate }.count());
			lastUpdate = now;
		}

		PrintServers(servers);
		success = servers.GetNumAnswered() > 0;

		if (!success)
		{
			gLog.Error("No {} server answered", game.name);
		}
	}

	engine.Terminate();

	return success;
}

//////////////////////////////////////////////////////////////////////////
// Each game's outcome on stdout, its problems on stderr; the window would show both in its download list. No keys
// means every game that is missing or has an update.
bool CSession::RunDownload(std::span<std::string const> keys)
{
	std::mutex mutex{};
	std::condition_variable wake{};
	bool hasResults{ false };
	Download::CGameDownloads downloads{};
	Download::CLatestRelease latestRelease{};
	auto const onResults{ [&mutex, &wake, &hasResults]()
	{
		{
			std::lock_guard const lock{ mutex };

			hasResults = true;
		}

		wake.notify_one();
	} };
	auto const waitUntilIdle{ [&]()
	{
		auto lastUpdate{ std::chrono::steady_clock::now() };

		while (downloads.GetPhase() != Download::EDownloadPhase::Idle || latestRelease.IsChecking())
		{
			{
				std::unique_lock lock{ mutex };

				wake.wait_for(lock, ListUpdateInterval, [&hasResults]() { return hasResults; });
				hasResults = false;
			}

			auto const now{ std::chrono::steady_clock::now() };

			downloads.Update(now);
			latestRelease.Update(now);
			Tge::gRuntime->Update(std::chrono::duration<float>{ now - lastUpdate }.count());
			lastUpdate = now;
		}
	} };

	bool success{ downloads.Initialize(m_userDir, m_cacheDir, Download::GetLookoutGamesSource(LKT_VERSION), onResults) };

	latestRelease.SetEnabled(Config::IsVersionCheckOn(m_configDir));

	if (!latestRelease.Initialize(Download::GetLookoutReleaseOrigin(LKT_VERSION), onResults))
	{
		gLog.Warning("Lookout cannot ask whether a newer version is out: the check could not be set up");
	}

	if (success)
	{
		latestRelease.Update(std::chrono::steady_clock::now());
		downloads.ReadIndex();
		waitUntilIdle();
		success = downloads.HasIndex();

		if (Download::IsNewerLookout(latestRelease.GetVersion(), LKT_VERSION))
		{
			gLog.Warning("Lookout {} is out: {}", latestRelease.GetVersion(), Download::LookoutReleaseUrl);
		}
	}
	else
	{
		gLog.Error("Cannot download games: {}", m_userDir.empty() ? "Lookout cannot locate its data folder" : "the download could not be set up");
	}

	std::vector<std::string> chosen{ keys.begin(), keys.end() };

	if (success && chosen.empty())
	{
		for (Download::SGameOffer const& offer : downloads.GetOffers())
		{
			if (offer.state == Download::EOfferState::NotInstalled || offer.state == Download::EOfferState::UpdateAvailable)
			{
				chosen.emplace_back(offer.key);
			}
		}
	}

	for (std::string const& key : chosen)
	{
		if (success && !std::ranges::contains(downloads.GetOffers(), key, &Download::SGameOffer::key))
		{
			gLog.Error("lookout-games has no game '{}'", key);
			success = false;
		}
	}

	if (success && !chosen.empty())
	{
		downloads.Download(chosen);
		waitUntilIdle();

		for (Download::SGameOffer const& offer : downloads.GetOffers())
		{
			if (std::ranges::contains(chosen, offer.key))
			{
				std::println("{}: {}", offer.key, (offer.state == Download::EOfferState::Installed) ? "installed" : "not installed");
			}
		}

		success = downloads.GetProblems().empty();
	}
	else if (success)
	{
		std::println("Every game is installed and up to date");
	}

	latestRelease.Terminate();
	downloads.Terminate();

	return success;
}

//////////////////////////////////////////////////////////////////////////
// Before the runtime starts, so its own start-up lines stay off stdout too, which carries only the table.
// Warnings and errors still reach the terminal, on stderr.
void CSession::StartListLogging()
{
	Tge::Logging::CLogSystem& logSystem{ Tge::Logging::GetLogSystem() };

	logSystem.SetEnabledTargets(Tge::Logging::ETarget::File | Tge::Logging::ETarget::Listeners);
	logSystem.RegisterListener(this, [](Tge::Logging::SLogMessage const& message)
	{
		if (message.level != Tge::Logging::ELogLevel::Info)
		{
			std::println(stderr, "{}", message.message);
		}
	});
}

//////////////////////////////////////////////////////////////////////////
void CSession::StopListLogging()
{
	Tge::Logging::CLogSystem& logSystem{ Tge::Logging::GetLogSystem() };

	logSystem.DispatchListeners();
	logSystem.UnregisterListener(this);
}

//////////////////////////////////////////////////////////////////////////
// Created here because the log system's own create_directories throws, which -fno-exceptions turns into a crash.
void CSession::PrepareDirectories()
{
	std::expected<std::filesystem::path, std::error_code> const configDir{ GetAppFolder(Tge::IO::EUserFolder::Config) };

	if (configDir.has_value())
	{
		std::error_code error{};

		std::filesystem::create_directories(*configDir, error);

		if (error.value() == 0)
		{
			m_configDir = configDir->string();
		}
		else
		{
			m_startupProblems.emplace_back(std::format("Cannot create the config directory '{}': {}", configDir->string(), error.message()));
		}
	}
	else
	{
		m_startupProblems.emplace_back(std::format("Cannot locate the config directory: {}", configDir.error().message()));
	}

	std::expected<std::filesystem::path, std::error_code> const stateDir{ GetAppFolder(Tge::IO::EUserFolder::State) };

	if (stateDir.has_value())
	{
		std::filesystem::path const logsDir{ *stateDir / "logs" };
		std::error_code error{};

		std::filesystem::create_directories(logsDir, error);

		if (error.value() == 0)
		{
			m_logsDir = logsDir.string();
		}
		else
		{
			m_startupProblems.emplace_back(std::format("Cannot create the log directory '{}', logging to the terminal only: {}", logsDir.string(), error.message()));
		}
	}
	else
	{
		m_startupProblems.emplace_back(std::format("Cannot locate the log directory, logging to the terminal only: {}", stateDir.error().message()));
	}

	std::expected<std::filesystem::path, std::error_code> const cacheDir{ GetAppFolder(Tge::IO::EUserFolder::Cache) };

	if (cacheDir.has_value())
	{
		m_cacheDir = *cacheDir;
	}
	else
	{
		m_startupProblems.emplace_back(std::format("Cannot locate the cache directory, so game icons are not kept: {}", cacheDir.error().message()));
	}
}

//////////////////////////////////////////////////////////////////////////
void CSession::ReportStartupProblems() const
{
	for (std::string const& problem : m_startupProblems)
	{
		gLog.Error("{}", problem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSession::ReportGameProblems(std::span<Query::SGameProblem const> problems) const
{
	for (Query::SGameProblem const& problem : problems)
	{
		gLog.Warning("{}", problem.text);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSession::PruneLogs() const
{
	if (!m_logsDir.empty())
	{
		std::vector<std::filesystem::path> logFiles{};
		std::error_code error{};

		for (std::filesystem::directory_iterator it{ m_logsDir, error }, end{}; error.value() == 0 && it != end; it.increment(error))
		{
			std::string const name{ it->path().filename().string() };

			if (name.starts_with(LogFilePrefix) && name.ends_with(LogFileExtension))
			{
				logFiles.emplace_back(it->path());
			}
		}

		if (error.value() == 0)
		{
			std::ranges::sort(logFiles, std::ranges::greater{});

			for (size_t index{ MaxLogFiles }; index < logFiles.size(); ++index)
			{
				std::error_code removeError{};

				std::filesystem::remove(logFiles[index], removeError);

				if (removeError.value() != 0)
				{
					gLog.Warning("Cannot remove the old log file '{}': {}", logFiles[index].string(), removeError.message());
				}
			}
		}
		else
		{
			gLog.Warning("Cannot list the log directory '{}' to remove old logs: {}", m_logsDir, error.message());
		}
	}
}
} // namespace Lkt::Cli
