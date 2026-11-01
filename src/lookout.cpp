#include "lookout.hpp"
#include "cli/session.hpp"
#include "launch/launch_environment.hpp"
#include "ui/about_info.hpp"
#include <chrono>

namespace Lkt
{
namespace
{
constexpr std::chrono::milliseconds SettleTime{ 500 };
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CLookout::Run(Cli::CSession& session)
{
	m_browser.Initialize(session.GetConfigDir(), session.GetLogsDir(), Launch::ReadLaunchEnvironment());
	m_browser.SetGameProblems(session.GetGameProblems());

	Ui::SAboutInfo const about{ LKT_VERSION, session.GetConfigDir(), session.GetLogsDir() };
	bool const isReady{ m_application.Initialize(about, m_browser.GetSettings().window, session.GetUserDir(), session.GetCacheDir())
		&& m_browser.Start(m_application.MakeWakeCallback()) };

	if (isReady)
	{
		if (!session.GetUserDir().empty())
		{
			m_watcher.Initialize(session.GetUserDir(), SettleTime, m_application.MakeReloadCallback());
		}

		m_browser.Refresh();
		m_application.Run(m_browser, [this, &session]() { session.ReloadGames(m_browser); });
		m_watcher.Terminate();
		m_browser.SetWindowSettings(m_application.GetWindowSettings());
	}

	// The browser first: its engine thread may still wake the window, which must not reach a closed SDL.
	m_browser.Terminate();
	m_application.Terminate();

	return isReady;
}
} // namespace Lkt
