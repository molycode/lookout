#pragma once

#include "browser/browser.hpp"
#include "games/folder_watcher.hpp"
#include "net/query_engine.hpp"
#include "run_request.hpp"
#include "query/game_problem.hpp"
#include "ui/application.hpp"
#include <tge/non_copyable.hpp>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace Lkt
{
namespace Query
{
struct SGameDefinition;
} // namespace Query

class CLookout final : private Tge::SNoCopyNoMove
{
public:

	CLookout() = default;
	~CLookout() = default;

	// Problems from loading the games are logged once logging reaches its file.
	bool Run(SRunRequest const& request, std::filesystem::path const& userDir, std::span<Query::SGameProblem const> gameProblems);

private:

	bool RunWindow();
	bool RunList(Query::SGameDefinition const& game);
	bool RunDownload(std::span<std::string const> keys);
	void ReloadGames();
	void ReportGameProblems(std::span<Query::SGameProblem const> problems) const;
	void StartListLogging();
	void StopListLogging();
	void PrepareDirectories();
	void ReportStartupProblems() const;
	void PruneLogs() const;

	std::string m_logsDir;
	std::string m_configDir;
	std::vector<std::string> m_startupProblems;
	std::filesystem::path m_userDir;
	std::filesystem::path m_cacheDir;
	std::vector<Query::SGameProblem> m_gameProblems;
	Games::CFolderWatcher m_watcher;
	Net::CQueryEngine m_engine;
	Browser::CBrowser m_browser;
	Ui::CApplication m_application;
};
} // namespace Lkt
