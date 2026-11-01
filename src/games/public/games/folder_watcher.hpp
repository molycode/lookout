#pragma once

#include <tge/io/directory_watch.hpp>
#include <tge/non_copyable.hpp>
#include <tge/threading/event_loop.hpp>
#include <tge/threading/timer_id.hpp>
#include <tge/threading/watch_id.hpp>
#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <system_error>

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

	std::expected<void, std::error_code> Arm();
	void Rearm();
	void WatchArmed();
	void ReadChanges();
	void ScheduleSettle();

	Tge::Threading::CEventLoop m_loop{};
	Tge::IO::CDirectoryWatch m_watch{};
	std::filesystem::path m_userDir{};
	std::filesystem::path m_watchedFolder{};
	std::chrono::milliseconds m_settleTime{ 0 };
	std::function<void()> m_onChanged{};
	std::optional<Tge::Threading::SWatchId> m_watchId{};
	std::optional<Tge::Threading::STimerId> m_settleTimer{};
	bool m_isRunning{ false };
};
} // namespace Lkt::Games
