#include "request_scheduler.hpp"
#include <algorithm>

namespace Lkt::Net
{
namespace
{
constexpr size_t MaxInFlight{ 256 };
constexpr uint32_t MaxAttempts{ 2 };
constexpr Clock::duration StepTimeout{ std::chrono::seconds{ 1 } };
constexpr Clock::duration Deadline{ std::chrono::seconds{ 5 } };
// One server's whole conversation; a server that sends more is broken or hostile.
constexpr uint32_t MaxDatagrams{ 64 };
constexpr size_t MaxBytes{ 256u << 10 };

constexpr Clock::duration SendInterval{ std::chrono::milliseconds{ 2 } };
constexpr Clock::duration MaxBurst{ SendInterval * 8 };

//////////////////////////////////////////////////////////////////////////
bool IsSame(SServerRequest const& lhs, SServerRequest const& rhs)
{
	return lhs.game == rhs.game && lhs.address == rhs.address;
}

//////////////////////////////////////////////////////////////////////////
bool CanResend(SInFlightRequest const& inFlight)
{
	return !inFlight.isStepAnswered && inFlight.request.numAttempts < MaxAttempts;
}

//////////////////////////////////////////////////////////////////////////
// When the clock ends the conversation, whatever the script does next.
Clock::time_point GetEnd(SInFlightRequest const& inFlight)
{
	Clock::time_point end{ inFlight.startedAt + Deadline };

	if (inFlight.quiet.has_value())
	{
		end = std::min(end, inFlight.lastDatagramAt + *inFlight.quiet);
	}

	if (!inFlight.isStepAnswered && !CanResend(inFlight))
	{
		end = std::min(end, inFlight.stepSentAt + StepTimeout);
	}

	return end;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CRequestScheduler::Add(SServerRequest const& request)
{
	m_pending.emplace_back(request);
}

//////////////////////////////////////////////////////////////////////////
// Resends go first, as a retry did when it rejoined the front of the queue.
void CRequestScheduler::TakeDue(Clock::time_point now, std::vector<SServerRequest>& due)
{
	m_nextSendAt = std::max(m_nextSendAt, now - MaxBurst);

	for (SInFlightRequest& inFlight : m_inFlight)
	{
		if (m_nextSendAt <= now && CanResend(inFlight) && now - inFlight.stepSentAt >= StepTimeout)
		{
			++inFlight.request.numAttempts;
			inFlight.stepSentAt = now;
			due.emplace_back(inFlight.request);
			m_nextSendAt += SendInterval;
		}
	}

	auto it{ m_pending.begin() };

	while (it != m_pending.end() && m_inFlight.size() < MaxInFlight && m_nextSendAt <= now)
	{
		if (IsLeased(it->address))
		{
			++it;
		}
		else
		{
			SInFlightRequest inFlight{};

			inFlight.request = *it;
			inFlight.request.numAttempts = 1;
			inFlight.startedAt = now;
			inFlight.stepSentAt = now;
			m_inFlight.emplace_back(inFlight);
			due.emplace_back(inFlight.request);
			it = m_pending.erase(it);
			m_nextSendAt += SendInterval;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// A datagram past the caps is not counted: the conversation ends on it.
std::optional<SAnsweredRequest> CRequestScheduler::OnDatagram(Query::SServerAddress const& address, size_t size, Clock::time_point now)
{
	std::optional<SAnsweredRequest> answered{};
	auto const it{ std::ranges::find(m_inFlight, address, [](SInFlightRequest const& inFlight) { return inFlight.request.address; }) };

	if (it != m_inFlight.end())
	{
		bool const isOverCap{ it->numDatagrams >= MaxDatagrams || it->numBytes + size > MaxBytes };

		if (!it->hasAnswered)
		{
			it->roundTrip = now - it->stepSentAt;
			it->hasAnswered = true;
		}

		if (!isOverCap)
		{
			++it->numDatagrams;
			it->numBytes += size;
		}

		it->lastDatagramAt = now;
		answered = SAnsweredRequest{ it->request, it->roundTrip, isOverCap };
	}

	return answered;
}

//////////////////////////////////////////////////////////////////////////
void CRequestScheduler::MarkStepAnswered(SServerRequest const& request)
{
	SInFlightRequest* const pInFlight{ Find(request) };

	if (pInFlight != nullptr)
	{
		pInFlight->isStepAnswered = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRequestScheduler::BeginStep(SServerRequest const& request, Clock::time_point now)
{
	SInFlightRequest* const pInFlight{ Find(request) };

	if (pInFlight != nullptr)
	{
		pInFlight->request.numAttempts = 1;
		pInFlight->stepSentAt = now;
		pInFlight->isStepAnswered = false;
		pInFlight->quiet.reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CRequestScheduler::SetQuiet(SServerRequest const& request, Clock::duration quiet)
{
	SInFlightRequest* const pInFlight{ Find(request) };

	if (pInFlight != nullptr)
	{
		pInFlight->quiet = quiet;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRequestScheduler::Remove(SServerRequest const& request)
{
	std::erase_if(m_inFlight, [&request](SInFlightRequest const& inFlight) { return IsSame(inFlight.request, request); });
}

//////////////////////////////////////////////////////////////////////////
void CRequestScheduler::TakeEnded(Clock::time_point now, std::vector<SEndedRequest>& ended)
{
	auto const hasEnded{ [now](SInFlightRequest const& inFlight) { return GetEnd(inFlight) <= now; } };

	for (SInFlightRequest const& inFlight : m_inFlight)
	{
		if (hasEnded(inFlight))
		{
			ended.emplace_back(inFlight.request, inFlight.roundTrip, inFlight.hasAnswered);
		}
	}

	std::erase_if(m_inFlight, hasEnded);
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
// A pending request whose address is leased waits for no time of its own, or the loop would wake at once, over and
// over, until the lease ends; the end of the lease comes with an update anyway.
std::optional<Clock::time_point> CRequestScheduler::GetNextDeadline() const
{
	std::optional<Clock::time_point> deadline{};
	auto const consider{ [&deadline](Clock::time_point when) { deadline = deadline.has_value() ? std::min(*deadline, when) : when; } };
	bool const canStart{ m_inFlight.size() < MaxInFlight
		&& std::ranges::any_of(m_pending, [this](SServerRequest const& request) { return !IsLeased(request.address); }) };

	if (canStart)
	{
		consider(m_nextSendAt);
	}

	for (SInFlightRequest const& inFlight : m_inFlight)
	{
		consider(GetEnd(inFlight));

		if (CanResend(inFlight))
		{
			consider(std::max(inFlight.stepSentAt + StepTimeout, m_nextSendAt));
		}
	}

	return deadline;
}

//////////////////////////////////////////////////////////////////////////
SInFlightRequest* CRequestScheduler::Find(SServerRequest const& request)
{
	auto const it{ std::ranges::find_if(m_inFlight, [&request](SInFlightRequest const& inFlight) { return IsSame(inFlight.request, request); }) };

	return (it != m_inFlight.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
bool CRequestScheduler::IsLeased(Query::SServerAddress const& address) const
{
	return std::ranges::contains(m_inFlight, address, [](SInFlightRequest const& inFlight) { return inFlight.request.address; });
}
} // namespace Lkt::Net
