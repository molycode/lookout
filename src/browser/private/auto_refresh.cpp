#include "browser/auto_refresh.hpp"

namespace Lkt::Browser
{
//////////////////////////////////////////////////////////////////////////
void CAutoRefresh::OnRefreshStarted(Query::EGame game, Net::Clock::time_point now)
{
	m_startedAt[static_cast<size_t>(game)] = now;
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
