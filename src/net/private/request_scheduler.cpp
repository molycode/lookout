#include "request_scheduler.hpp"
#include <algorithm>

namespace Lkt::Net
{
namespace
{
constexpr size_t MaxInFlight{ 256 };
constexpr uint32_t MaxAttempts{ 2 };
constexpr Clock::duration Timeout{ std::chrono::seconds{ 1 } };

constexpr Clock::duration SendInterval{ std::chrono::milliseconds{ 2 } };
constexpr Clock::duration MaxBurst{ SendInterval * 8 };
} // namespace

//////////////////////////////////////////////////////////////////////////
void CRequestScheduler::Add(SServerRequest const& request)
{
	m_pending.emplace_back(request);
}

//////////////////////////////////////////////////////////////////////////
void CRequestScheduler::TakeDue(Clock::time_point now, std::vector<SServerRequest>& due)
{
	m_nextSendAt = std::max(m_nextSendAt, now - MaxBurst);

	while (!m_pending.empty() && m_inFlight.size() < MaxInFlight && m_nextSendAt <= now)
	{
		SServerRequest request{ m_pending.front() };

		m_pending.pop_front();
		++request.numAttempts;
		m_inFlight.emplace_back(request, now);
		due.emplace_back(request);
		m_nextSendAt += SendInterval;
	}
}

//////////////////////////////////////////////////////////////////////////
std::optional<SAnsweredRequest> CRequestScheduler::Answer(Query::SServerAddress const& address, Clock::time_point now)
{
	std::optional<SAnsweredRequest> answered{};

	auto const it{ std::ranges::find(m_inFlight, address, [](SInFlightRequest const& inFlight) { return inFlight.request.address; }) };

	if (it != m_inFlight.end())
	{
		answered = SAnsweredRequest{ it->request, now - it->sentAt };
		m_inFlight.erase(it);
	}

	return answered;
}

//////////////////////////////////////////////////////////////////////////
void CRequestScheduler::TakeExpired(Clock::time_point now, std::vector<SServerRequest>& expired)
{
	auto const isExpired{ [now](SInFlightRequest const& inFlight) { return now - inFlight.sentAt >= Timeout; } };

	for (SInFlightRequest const& inFlight : m_inFlight)
	{
		if (isExpired(inFlight))
		{
			if (inFlight.request.numAttempts < MaxAttempts)
			{
				m_pending.emplace_front(inFlight.request);
			}
			else
			{
				expired.emplace_back(inFlight.request);
			}
		}
	}

	std::erase_if(m_inFlight, isExpired);
}

//////////////////////////////////////////////////////////////////////////
// Taken out of flight before it could be sent, so it neither expires nor is retried.
void CRequestScheduler::Abandon(SServerRequest const& request)
{
	std::erase_if(m_inFlight, [&request](SInFlightRequest const& inFlight)
	{
		return inFlight.request.game == request.game && inFlight.request.address == request.address;
	});
}

//////////////////////////////////////////////////////////////////////////
void CRequestScheduler::Cancel(Query::EGame game)
{
	std::erase_if(m_pending, [game](SServerRequest const& request) { return request.game == game; });
	std::erase_if(m_inFlight, [game](SInFlightRequest const& inFlight) { return inFlight.request.game == game; });
}

//////////////////////////////////////////////////////////////////////////
bool CRequestScheduler::HasWork(Query::EGame game) const
{
	return std::ranges::any_of(m_pending, [game](SServerRequest const& request) { return request.game == game; })
		|| std::ranges::any_of(m_inFlight, [game](SInFlightRequest const& inFlight) { return inFlight.request.game == game; });
}

//////////////////////////////////////////////////////////////////////////
bool CRequestScheduler::IsQueued(Query::EGame game, Query::SServerAddress const& address) const
{
	auto const isRequest{ [game, &address](SServerRequest const& request) { return request.game == game && request.address == address; } };

	return std::ranges::any_of(m_pending, isRequest)
		|| std::ranges::any_of(m_inFlight, [&isRequest](SInFlightRequest const& inFlight) { return isRequest(inFlight.request); });
}

//////////////////////////////////////////////////////////////////////////
std::optional<Clock::time_point> CRequestScheduler::GetNextDeadline() const
{
	std::optional<Clock::time_point> deadline{};

	if (!m_pending.empty() && m_inFlight.size() < MaxInFlight)
	{
		deadline = m_nextSendAt;
	}

	for (SInFlightRequest const& inFlight : m_inFlight)
	{
		Clock::time_point const expiresAt{ inFlight.sentAt + Timeout };

		deadline = deadline.has_value() ? std::min(deadline.value(), expiresAt) : expiresAt;
	}

	return deadline;
}
} // namespace Lkt::Net
