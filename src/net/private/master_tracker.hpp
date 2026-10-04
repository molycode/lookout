#pragma once

#include "master_id.hpp"
#include "master_outcome.hpp"
#include "master_query.hpp"
#include "master_record.hpp"
#include "net/clock.hpp"
#include "query/master_endpoint.hpp"
#include <tge/non_copyable.hpp>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Lkt::Net
{
// Each master of a refresh: resolve in time, ask, ask once more, then take datagrams until it has been quiet a while.
// A generation tells this refresh's DNS answers from those of one it replaced.
class CMasterTracker final : private Tge::SNoCopyNoMove
{
public:

	CMasterTracker() = default;
	~CMasterTracker() = default;

	void Begin(Query::EGame game, uint32_t generation, std::span<Query::SMasterEndpoint const> masters, Clock::time_point now);
	void OnResolved(Query::EGame game, uint32_t generation, size_t index, std::expected<uint32_t, std::string> const& result, Clock::time_point now);
	void Update(Clock::time_point now, std::vector<SMasterQuery>& queries, std::vector<SMasterOutcome>& outcomes);

	// The first asked master at that address takes the datagram; every one there hears it.
	std::optional<SMasterId> OnDatagram(Query::SServerAddress const& source, Clock::time_point now);

	size_t AdmitEntries(SMasterId const& master, size_t numEntries);
	void Fail(SMasterId const& master, std::string failure, std::vector<SMasterOutcome>& outcomes);

	void Cancel(Query::EGame game);

	bool HasWork(Query::EGame game) const;
	std::optional<Clock::time_point> GetNextDeadline() const;

private:

	std::vector<SMasterRecord> m_records;
};
} // namespace Lkt::Net
