#pragma once

#include <tge/non_copyable.hpp>
#include <tge/threading/event_loop.hpp>
#include <tge/threading/timer_id.hpp>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>

namespace Lkt::Games
{
class CFolderWatcher final : private Tge::SNoCopyNoMove
{
public:

	CFolderWatcher() = default;
	~CFolderWatcher() = default;

	bool Initialize(std::filesystem::path const& userDir, std::chrono::milliseconds settleTime, std::function<void()> onChanged);
	void Terminate();

private:

	bool Rearm();
	bool AddWatch(std::filesystem::path const& folder);
	void ReadEvents();
	void ScheduleSettle();

	Tge::Threading::CEventLoop m_loop;
	std::filesystem::path m_userDir;
	std::chrono::milliseconds m_settleTime{ 0 };
	std::function<void()> m_onChanged;
	std::map<int, std::filesystem::path> m_watches;
	std::optional<Tge::Threading::STimerId> m_settleTimer;
	int m_inotify{ -1 };
	bool m_isRunning{ false };
};
} // namespace Lkt::Games
