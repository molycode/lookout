#pragma once

#include "net/clock.hpp"
#include "query/game.hpp"
#include <tge/non_copyable.hpp>
#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace Lkt::Browser
{
// Timed from the start of a game's last full refresh, so a manual Refresh restarts it; a zero interval is off.
class CAutoRefresh final : private Tge::SNoCopyNoMove
{
public:

	CAutoRefresh() = default;
	~CAutoRefresh() = default;

	void Initialize(size_t numGames);
	void OnRefreshStarted(Query::EGame game, Net::Clock::time_point now);
	void Remap(std::span<std::optional<size_t> const> keptFrom);

	// A game never refreshed has no deadline: the browser refreshes it when it is first selected.
	std::optional<Net::Clock::time_point> GetDeadline(Query::EGame game, std::chrono::seconds interval) const;

private:

	std::vector<std::optional<Net::Clock::time_point>> m_startedAt;
};
} // namespace Lkt::Browser
