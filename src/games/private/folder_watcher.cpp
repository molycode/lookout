#include "games/folder_watcher.hpp"
#include "loader_input.hpp"
#include "loggers.hpp"
#include <algorithm>
#include <utility>

namespace Lkt::Games
{
namespace
{
//////////////////////////////////////////////////////////////////////////
// The folder itself when it exists, else its nearest ancestor that does.
std::filesystem::path FindNearestFolder(std::filesystem::path const& folder)
{
	std::error_code error{};
	std::filesystem::path nearest{ folder };

	while (nearest.has_relative_path() && !std::filesystem::is_directory(nearest, error))
	{
		nearest = nearest.parent_path();
	}

	return nearest;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// The watch starts before the loop runs: what changes in between waits in the system, so nothing is missed.
bool CFolderWatcher::Initialize(std::filesystem::path const& userDir, std::chrono::milliseconds settleTime, std::function<void()> onChanged)
{
	m_userDir = userDir;
	m_settleTime = settleTime;
	m_onChanged = std::move(onChanged);

	std::expected<void, std::error_code> const armed{ Arm() };

	if (!armed.has_value())
	{
		gLog.Warning("Cannot watch '{}', so changed game descriptions need a restart: {}", m_userDir.string(), armed.error().message());
	}
	else if (m_loop.Initialize("LookoutWatch"))
	{
		m_isRunning = true;
		m_loop.Post([this]() { WatchArmed(); });
	}
	else
	{
		gLog.Warning("Cannot wait for changes to '{}', so changed game descriptions need a restart", m_userDir.string());
		m_watch.Terminate();
	}

	return m_isRunning;
}

//////////////////////////////////////////////////////////////////////////
void CFolderWatcher::Terminate()
{
	if (m_isRunning)
	{
		m_loop.Terminate();
		m_isRunning = false;
	}

	m_watch.Terminate();
	m_watchId.reset();
	m_settleTimer.reset();
}

//////////////////////////////////////////////////////////////////////////
// The user folder's whole tree; until it exists, its nearest existing ancestor alone, so that nothing has to be created
// to be watched. A folder made toward the user folder between the look and the watch sends no event, so the look is
// repeated.
std::expected<void, std::error_code> CFolderWatcher::Arm()
{
	std::expected<void, std::error_code> armed{};
	bool isSettled{ false };

	while (armed.has_value() && !isSettled)
	{
		m_watchedFolder = FindNearestFolder(m_userDir);
		armed = m_watch.Initialize(m_watchedFolder, (m_watchedFolder == m_userDir) ? Tge::IO::EWatchScope::Tree : Tge::IO::EWatchScope::Folder);
		isSettled = !armed.has_value() || FindNearestFolder(m_userDir) == m_watchedFolder;

		if (!isSettled)
		{
			m_watch.Terminate();
		}
	}

	return armed;
}

//////////////////////////////////////////////////////////////////////////
void CFolderWatcher::Rearm()
{
	if (m_watchId.has_value())
	{
		m_loop.Unwatch(*m_watchId);
		m_watchId.reset();
	}

	m_watch.Terminate();

	std::expected<void, std::error_code> const armed{ Arm() };

	if (armed.has_value())
	{
		WatchArmed();
	}
	else
	{
		gLog.Warning("Cannot watch '{}', so changed game descriptions need a restart: {}", m_userDir.string(), armed.error().message());
	}
}

//////////////////////////////////////////////////////////////////////////
void CFolderWatcher::WatchArmed()
{
	m_watchId = m_loop.Watch(m_watch.GetWaitable(), [this]() { ReadChanges(); });

	if (!m_watchId.has_value())
	{
		gLog.Warning("Cannot wait for changes to '{}', so changed game descriptions need a restart", m_userDir.string());
	}
}

//////////////////////////////////////////////////////////////////////////
// A watch that ended took the user folder or an ancestor with it, so anything may have changed. While an ancestor is
// watched, only a step toward the user folder matters, and the games change once the user folder is reached.
void CFolderWatcher::ReadChanges()
{
	std::expected<Tge::IO::SDirectoryChanges, std::error_code> const changes{ m_watch.ReadChanges() };
	bool const isWatchingUserDir{ m_watchedFolder == m_userDir };
	bool isChanged{ !changes.has_value() || changes->hasOverflowed };
	bool shouldRearm{ !changes.has_value() };

	if (changes.has_value() && isWatchingUserDir)
	{
		isChanged = isChanged || std::ranges::any_of(changes->paths, IsLoaderInput);
	}
	else if (changes.has_value())
	{
		shouldRearm = FindNearestFolder(m_userDir) != m_watchedFolder;
	}

	if (shouldRearm)
	{
		Rearm();
		isChanged = isChanged || (!isWatchingUserDir && m_watchedFolder == m_userDir);
	}

	if (isChanged)
	{
		ScheduleSettle();
	}
}

//////////////////////////////////////////////////////////////////////////
// A save or a checkout writes many files: each change restarts the wait, so the reload comes once they settle.
void CFolderWatcher::ScheduleSettle()
{
	if (m_settleTimer.has_value())
	{
		m_loop.CancelTimer(*m_settleTimer);
	}

	m_settleTimer = m_loop.ScheduleAt(std::chrono::steady_clock::now() + m_settleTime, [this]()
	{
		m_settleTimer.reset();
		m_onChanged();
	});
}
} // namespace Lkt::Games
