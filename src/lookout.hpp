#pragma once

#include "browser/browser.hpp"
#include "net/query_engine.hpp"
#include "ui/application.hpp"
#include <tge/non_copyable.hpp>
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

	bool Run(Query::SGameDefinition const* pListGame);

private:

	bool RunWindow();
	bool RunList(Query::SGameDefinition const& game);
	void StartListLogging();
	void StopListLogging();
	void PrepareDirectories();
	void ReportStartupProblems() const;
	void PruneLogs() const;

	std::string m_logsDir;
	std::string m_configDir;
	std::vector<std::string> m_startupProblems;
	Net::CQueryEngine m_engine;
	Browser::CBrowser m_browser;
	Ui::CApplication m_application;
};
} // namespace Lkt
