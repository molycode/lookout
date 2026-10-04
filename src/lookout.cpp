#include "lookout.hpp"
#include "app_dir_name.hpp"
#include "loggers.hpp"
#include "run_context.hpp"
#include "browser/server_list.hpp"
#include "config/xdg_paths.hpp"
#include "games/load_games.hpp"
#include "launch/launch_environment.hpp"
#include "query/game_definition.hpp"
#include <tge/logging/log_system.hpp>
#include <tge/module/runtime.hpp>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <format>
#include <mutex>
#include <print>
#include <system_error>
#include <utility>
#include <vector>

namespace Lkt
{
namespace
{
constexpr size_t MaxLogFiles{ 10 };
constexpr std::chrono::milliseconds SettleTime{ 500 };

// tge's runtime names every log file "tge_<date>_<time>.log", so the name orders them by age.
constexpr std::string_view LogFilePrefix{ "tge_" };
constexpr std::string_view LogFileExtension{ ".log" };

constexpr std::chrono::milliseconds ListUpdateInterval{ 250 };

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

		std::println("{} {}/{} {} {}", Query::FormatAddress(pEntry->address), summary.numPlayers, summary.maxPlayers, summary.map, summary.name.plain);
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CLookout::Run(Query::SGameDefinition const* pListGame, std::filesystem::path const& userDir, std::span<std::string const> gameProblems)
{
	PrepareDirectories();
	m_userDir = userDir;
	m_gameProblems.assign(gameProblems.begin(), gameProblems.end());

	bool const isListing{ pListGame != nullptr };

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

		success = isListing ? RunList(*pListGame) : RunWindow();

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
bool CLookout::RunWindow()
{
	m_browser.Initialize(m_configDir, m_logsDir, Launch::ReadLaunchEnvironment());
	m_browser.SetGameProblems(m_gameProblems);

	Ui::SAboutInfo const about{ LKT_VERSION, m_configDir, m_logsDir };
	bool const isReady{ m_application.Initialize(about, m_browser.GetSettings().window, m_userDir) && m_browser.Start(m_application.MakeWakeCallback()) };

	if (isReady)
	{
		if (!m_userDir.empty())
		{
			m_watcher.Initialize(m_userDir, SettleTime, m_application.MakeReloadCallback());
		}

		m_browser.Refresh();
		m_application.Run(m_browser, [this]() { ReloadGames(); });
		m_watcher.Terminate();
		m_browser.SetWindowSettings(m_application.GetWindowSettings());
	}

	// The browser first: its engine thread may still wake the window, which must not reach a closed SDL.
	m_browser.Terminate();
	m_application.Terminate();

	return isReady;
}

//////////////////////////////////////////////////////////////////////////
// A save in the editor reloads at once and again when the watcher sees it; the second has nothing new to say.
void CLookout::ReloadGames()
{
	Games::SGameContent content{ Games::LoadGames(m_userDir) };
	size_t const numGames{ content.games.size() };
	bool const hasNewProblems{ !std::ranges::equal(content.problems, m_browser.GetGameProblems()) };
	bool const isReplaced{ m_browser.ReplaceCatalog(std::move(content.protocols), std::move(content.games)) };

	if (isReplaced || hasNewProblems)
	{
		ReportGameProblems(content.problems);
		gLog.Info("Reloaded the game descriptions: {} games", numGames);
	}

	m_browser.SetGameProblems(std::move(content.problems));
}

//////////////////////////////////////////////////////////////////////////
bool CLookout::RunList(Query::SGameDefinition const& game)
{
	std::mutex mutex{};
	std::condition_variable wake{};
	bool hasEvents{ false };
	bool success{ false };

	bool const isEngineReady{ m_engine.Initialize([&mutex, &wake, &hasEvents]()
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

		servers.BeginRefresh(m_engine.Refresh(game.game, {}));

		while (!isFinished)
		{
			{
				std::unique_lock lock{ mutex };

				wake.wait_for(lock, ListUpdateInterval, [&hasEvents]() { return hasEvents; });
				hasEvents = false;
			}

			m_engine.TakeEvents(events);

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

	m_engine.Terminate();

	return success;
}

//////////////////////////////////////////////////////////////////////////
// Before the runtime starts, so its own start-up lines stay off stdout too, which carries only the table.
// Warnings and errors still reach the terminal, on stderr.
void CLookout::StartListLogging()
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
void CLookout::StopListLogging()
{
	Tge::Logging::CLogSystem& logSystem{ Tge::Logging::GetLogSystem() };

	logSystem.DispatchListeners();
	logSystem.UnregisterListener(this);
}

//////////////////////////////////////////////////////////////////////////
// Created here because the log system's own create_directories throws, which -fno-exceptions turns into a crash.
void CLookout::PrepareDirectories()
{
	std::expected<std::filesystem::path, Config::EXdgError> const configHome{ Config::GetConfigHome() };

	if (configHome.has_value())
	{
		std::filesystem::path const configDir{ *configHome / AppDirName };
		std::error_code error{};

		std::filesystem::create_directories(configDir, error);

		if (error.value() == 0)
		{
			m_configDir = configDir.string();
		}
		else
		{
			m_startupProblems.emplace_back(std::format("Cannot create the config directory '{}': {}", configDir.string(), error.message()));
		}
	}
	else
	{
		m_startupProblems.emplace_back(std::format("Cannot locate the config directory: {}", Config::ToString(configHome.error())));
	}

	std::expected<std::filesystem::path, Config::EXdgError> const stateHome{ Config::GetStateHome() };

	if (stateHome.has_value())
	{
		std::filesystem::path const logsDir{ *stateHome / AppDirName / "logs" };
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
		m_startupProblems.emplace_back(std::format("Cannot locate the log directory, logging to the terminal only: {}", Config::ToString(stateHome.error())));
	}
}

//////////////////////////////////////////////////////////////////////////
void CLookout::ReportStartupProblems() const
{
	for (std::string const& problem : m_startupProblems)
	{
		gLog.Error("{}", problem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CLookout::ReportGameProblems(std::span<std::string const> problems) const
{
	for (std::string const& problem : problems)
	{
		gLog.Warning("{}", problem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CLookout::PruneLogs() const
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
} // namespace Lkt
