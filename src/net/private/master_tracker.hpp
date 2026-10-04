#pragma once

#include "master_id.hpp"
#include "master_outcome.hpp"
#include "master_query.hpp"
#include "master_record.hpp"
#include "net/clock.hpp"
#include "query/master_endpoint.hpp"
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Lkt::Net
{
// The clock of master conversations: resolve in time, ask, ask once more when a step goes unanswered, and end when
// the list is complete (quiet, or done), cut short, or too slow. Only a complete list counts as answered. A
// generation tells this refresh's DNS answers from those of one it replaced.
class CMasterTracker final : private Tge::SNoCopyNoMove
{
public:

	CMasterTracker() = default;
	~CMasterTracker() = default;

	void Begin(Query::EGame game, uint32_t generation, std::span<Query::SMasterEndpoint const> masters, Clock::time_point now);
	void OnResolved(Query::EGame game, uint32_t generation, size_t index, std::expected<uint32_t, std::string> const& result, Clock::time_point now);
	// Queries to send: a master's first, or a resend of a step nobody answered.
	void Update(Clock::time_point now, std::vector<SMasterQuery>& queries, std::vector<SMasterOutcome>& outcomes);

	void OnDatagram(SMasterId const& master, Clock::time_point now);
	// Only a datagram the script made use of: a stray reply to an earlier page must not stop this one's resend.
	void MarkStepAnswered(SMasterId const& master);
	void BeginStep(SMasterId const& master, Clock::time_point now);
	void SetQuiet(SMasterId const& master, Clock::duration quiet);
	size_t AdmitEntries(SMasterId const& master, size_t numEntries);
	// Reported at once: an ended master arms no timer, so waiting for the next update could leave a refresh hanging.
	void Finish(SMasterId const& master, std::vector<SMasterOutcome>& outcomes);
	void Fail(SMasterId const& master, std::string failure, std::vector<SMasterOutcome>& outcomes);

	void Cancel(Query::EGame game);

	bool IsAsking(SMasterId const& master) const;
	bool HasWork(Query::EGame game) const;
	std::optional<Clock::time_point> GetNextDeadline() const;

private:

	SMasterRecord* Find(SMasterId const& master);
	void End(SMasterRecord& record, std::string failure, std::vector<SMasterOutcome>& outcomes);

	std::vector<SMasterRecord> m_records;
};
} // namespace Lkt::Net
