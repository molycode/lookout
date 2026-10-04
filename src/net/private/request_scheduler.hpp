#pragma once

#include "answered_request.hpp"
#include "ended_request.hpp"
#include "in_flight_request.hpp"
#include "server_request.hpp"
#include "net/clock.hpp"
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

namespace Lkt::Net
{
// The clock of server conversations: paces new ones and resends, leases each address to one conversation at a time,
// and ends a conversation whose step goes unanswered, that falls quiet, or that runs past its deadline. The pump
// tells it what each script decided. Time is always passed in, so the schedule is the same in a test as on the wire.
class CRequestScheduler final : private Tge::SNoCopyNoMove
{
public:

	CRequestScheduler() = default;
	~CRequestScheduler() = default;

	void Add(SServerRequest const& request);
	// New conversations (one attempt) and resends of a step nobody answered (more), drawing on one pace.
	void TakeDue(Clock::time_point now, std::vector<SServerRequest>& due);
	std::optional<SAnsweredRequest> OnDatagram(Query::SServerAddress const& address, size_t size, Clock::time_point now);
	// Only a datagram the script made use of: a stray reply to an earlier step must not stop this one's resend.
	void MarkStepAnswered(SServerRequest const& request);
	void BeginStep(SServerRequest const& request, Clock::time_point now);
	void SetQuiet(SServerRequest const& request, Clock::duration quiet);
	void Remove(SServerRequest const& request);
	void TakeEnded(Clock::time_point now, std::vector<SEndedRequest>& ended);
	void Cancel(Query::EGame game);

	bool HasWork(Query::EGame game) const;
	bool IsQueued(Query::EGame game, Query::SServerAddress const& address) const;
	std::optional<Clock::time_point> GetNextDeadline() const;

private:

	SInFlightRequest* Find(SServerRequest const& request);
	bool IsLeased(Query::SServerAddress const& address) const;

	std::deque<SServerRequest> m_pending;
	std::vector<SInFlightRequest> m_inFlight;
	Clock::time_point m_nextSendAt{};
};
} // namespace Lkt::Net
