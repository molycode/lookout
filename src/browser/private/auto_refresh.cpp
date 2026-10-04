#include "browser/auto_refresh.hpp"
#include <utility>

namespace Lkt::Browser
{
//////////////////////////////////////////////////////////////////////////
void CAutoRefresh::Initialize(size_t numGames)
{
	m_startedAt.assign(numGames, std::nullopt);
}

//////////////////////////////////////////////////////////////////////////
void CAutoRefresh::OnRefreshStarted(Query::EGame game, Net::Clock::time_point now)
{
	m_startedAt[static_cast<size_t>(game)] = now;
}

//////////////////////////////////////////////////////////////////////////
// For each game of a new catalog, the game of the old one whose timer it keeps, if any.
void CAutoRefresh::Remap(std::span<std::optional<size_t> const> keptFrom)
{
	std::vector<std::optional<Net::Clock::time_point>> startedAt(keptFrom.size());

	for (size_t index{ 0 }; index < keptFrom.size(); ++index)
	{
		if (keptFrom[index].has_value())
		{
			startedAt[index] = m_startedAt[*keptFrom[index]];
		}
	}

	m_startedAt = std::move(startedAt);
}

//////////////////////////////////////////////////////////////////////////
std::optional<Net::Clock::time_point> CAutoRefresh::GetDeadline(Query::EGame game, std::chrono::seconds interval) const
{
	std::optional<Net::Clock::time_point> const& startedAt{ m_startedAt[static_cast<size_t>(game)] };
	std::optional<Net::Clock::time_point> deadline{};

	if (interval.count() != 0 && startedAt.has_value())
	{
		deadline = *startedAt + interval;
	}

	return deadline;
}
} // namespace Lkt::Browser
