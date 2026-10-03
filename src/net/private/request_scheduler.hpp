#pragma once

#include "answered_request.hpp"
#include "in_flight_request.hpp"
#include "server_request.hpp"
#include "net/clock.hpp"
#include <tge/non_copyable.hpp>
#include <deque>
#include <optional>
#include <vector>

namespace Lkt::Net
{
// Paces server status requests and retries each once. Time is always passed in, so the schedule is the same in
// a test as on the wire.
class CRequestScheduler final : private Tge::SNoCopyNoMove
{
public:

	CRequestScheduler() = default;
	~CRequestScheduler() = default;

	void Add(SServerRequest const& request);
	void TakeDue(Clock::time_point now, std::vector<SServerRequest>& due);
	std::optional<SAnsweredRequest> Answer(Query::SServerAddress const& address, Clock::time_point now);
	void TakeExpired(Clock::time_point now, std::vector<SServerRequest>& expired);
	void Cancel(Query::EGame game);

	bool HasWork(Query::EGame game) const;
	bool IsQueued(Query::EGame game, Query::SServerAddress const& address) const;
	std::optional<Clock::time_point> GetNextDeadline() const;

private:

	std::deque<SServerRequest> m_pending;
	std::vector<SInFlightRequest> m_inFlight;
	Clock::time_point m_nextSendAt{};
};
} // namespace Lkt::Net
