#include "games/folder_watcher.hpp"
#include "loggers.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <string_view>
#include <sys/inotify.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace Lkt::Games
{
namespace
{
constexpr uint32_t WatchedEvents{ IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF
	| IN_ONLYDIR };

//////////////////////////////////////////////////////////////////////////
bool IsWithin(std::filesystem::path const& path, std::filesystem::path const& base)
{
	auto const [baseEnd, pathEnd]{ std::mismatch(base.begin(), base.end(), path.begin(), path.end()) };

	return baseEnd == base.end();
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// The watches are added before the loop runs: what changes in between waits in the descriptor, so nothing is missed.
bool CFolderWatcher::Initialize(std::filesystem::path const& userDir, std::chrono::milliseconds settleTime, std::function<void()> onChanged)
{
	m_userDir = userDir;
	m_settleTime = settleTime;
	m_onChanged = std::move(onChanged);
	m_inotify = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

	if (m_inotify >= 0)
	{
		Rearm();
		m_isRunning = m_loop.Initialize("LookoutWatch");
	}

	if (m_isRunning)
	{
		m_loop.Post([this]()
		{
			if (!m_loop.Watch(m_inotify, [this]() { ReadEvents(); }).has_value())
			{
				gLog.Warning("Cannot wait for changes to '{}', so changed game descriptions need a restart", m_userDir.string());
			}
		});
	}
	else
	{
		gLog.Warning("Cannot watch '{}', so changed game descriptions need a restart: {}", m_userDir.string(), std::strerror(errno));
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

	if (m_inotify >= 0)
	{
		::close(m_inotify);
		m_inotify = -1;
	}

	m_watches.clear();
	m_settleTimer.reset();
}

//////////////////////////////////////////////////////////////////////////
// The user folder, its two folders and each game's folder; until the user folder exists, its nearest existing ancestor,
// so that nothing has to be created to be watched. A folder newly watched may have been filled before its watch.
bool CFolderWatcher::Rearm()
{
	std::error_code error{};
	bool hasNewFolder{ false };

	if (std::filesystem::is_directory(m_userDir, error))
	{
		hasNewFolder = AddWatch(m_userDir) || hasNewFolder;
		hasNewFolder = AddWatch(m_userDir / "protocols") || hasNewFolder;
		hasNewFolder = AddWatch(m_userDir / "games") || hasNewFolder;

		for (std::filesystem::directory_iterator it{ m_userDir / "games", error }, end{}; error.value() == 0 && it != end; it.increment(error))
		{
			if (!it->path().filename().string().starts_with('.') && it->is_directory(error))
			{
				hasNewFolder = AddWatch(it->path()) || hasNewFolder;
			}
		}
	}
	else
	{
		std::filesystem::path ancestor{ m_userDir.parent_path() };

		while (ancestor.has_relative_path() && !std::filesystem::is_directory(ancestor, error))
		{
			ancestor = ancestor.parent_path();
		}

		AddWatch(ancestor);
	}

	return hasNewFolder;
}

//////////////////////////////////////////////////////////////////////////
// A folder that does not exist, or no longer does, is simply not watched.
bool CFolderWatcher::AddWatch(std::filesystem::path const& folder)
{
	int const watch{ ::inotify_add_watch(m_inotify, folder.c_str(), WatchedEvents) };
	bool const isNew{ watch >= 0 && !m_watches.contains(watch) };

	if (watch >= 0)
	{
		m_watches[watch] = folder;
	}

	return isNew;
}

//////////////////////////////////////////////////////////////////////////
// Dot entries are left out, as the loader leaves them: editors' swap files and a checkout's .git change all the time.
void CFolderWatcher::ReadEvents()
{
	alignas(inotify_event) std::array<char, 16 * 1024> buffer{};
	bool isChanged{ false };
	bool shouldRearm{ false };
	ssize_t count{ ::read(m_inotify, buffer.data(), buffer.size()) };

	while (count > 0)
	{
		size_t offset{ 0 };

		while (offset < static_cast<size_t>(count))
		{
			inotify_event const* const pEvent{ reinterpret_cast<inotify_event const*>(buffer.data() + offset) };
			std::string_view const name{ (pEvent->len > 0) ? std::string_view{ pEvent->name } : std::string_view{} };
			auto const watched{ m_watches.find(pEvent->wd) };

			if ((pEvent->mask & IN_Q_OVERFLOW) != 0)
			{
				isChanged = true;
				shouldRearm = true;
			}
			else if ((pEvent->mask & IN_IGNORED) != 0)
			{
				m_watches.erase(pEvent->wd);
			}
			else if (watched != m_watches.end() && !name.starts_with('.'))
			{
				std::filesystem::path const path{ name.empty() ? watched->second : watched->second / name };
				bool const isInside{ IsWithin(path, m_userDir) };

				isChanged = isChanged || isInside;
				shouldRearm = shouldRearm || (isInside && (pEvent->mask & IN_ISDIR) != 0) || (!isInside && IsWithin(m_userDir, path));
			}

			offset += sizeof(inotify_event) + pEvent->len;
		}

		count = ::read(m_inotify, buffer.data(), buffer.size());
	}

	if (count < 0 && errno != EAGAIN && errno != EINTR)
	{
		gLog.Warning("Cannot read the changes to '{}': {}", m_userDir.string(), std::strerror(errno));
	}

	if (shouldRearm)
	{
		isChanged = Rearm() || isChanged;
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
		Rearm();
		m_onChanged();
	});
}
} // namespace Lkt::Games
