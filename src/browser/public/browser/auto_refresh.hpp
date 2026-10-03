#pragma once

#include "net/clock.hpp"
#include "query/game.hpp"
#include <tge/non_copyable.hpp>
#include <array>
#include <chrono>
#include <optional>

namespace Lkt::Browser
{
// Timed from the start of a game's last full refresh, so a manual Refresh restarts it; a zero interval is off.
class CAutoRefresh final : private Tge::SNoCopyNoMove
{
public:

	CAutoRefresh() = default;
	~CAutoRefresh() = default;

	void OnRefreshStarted(Query::EGame game, Net::Clock::time_point now);

	// A game never refreshed has no deadline: the browser refreshes it when it is first selected.
	std::optional<Net::Clock::time_point> GetDeadline(Query::EGame game, std::chrono::seconds interval) const;

private:

	std::array<std::optional<Net::Clock::time_point>, Query::NumGames> m_startedAt;
};
} // namespace Lkt::Browser
