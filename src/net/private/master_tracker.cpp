#include "master_tracker.hpp"
#include <algorithm>
#include <format>

namespace Lkt::Net
{
namespace
{
constexpr uint32_t MaxAttempts{ 2 };
constexpr Clock::duration ResolveTimeout{ std::chrono::seconds{ 5 } };
constexpr Clock::duration ReplyTimeout{ std::chrono::seconds{ 2 } };
constexpr Clock::duration QuietPeriod{ std::chrono::milliseconds{ 1500 } };

// The busiest master measured lists about 1,000 servers; anything far beyond that is broken or hostile.
constexpr size_t MaxEntriesPerMaster{ 4096 };

//////////////////////////////////////////////////////////////////////////
bool IsListening(SMasterRecord const& record)
{
	return record.state == EMasterState::Querying || record.state == EMasterState::Receiving;
}

//////////////////////////////////////////////////////////////////////////
bool IsWaiting(SMasterRecord const& record)
{
	return IsListening(record) || record.state == EMasterState::Resolving;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::Begin(Query::EGame game, uint32_t generation, std::span<Query::SMasterEndpoint const> masters, Clock::time_point now)
{
	Cancel(game);

	for (Query::SMasterEndpoint const& master : masters)
	{
		SMasterRecord record{};

		record.game = game;
		record.generation = generation;
		record.host = master.host;
		record.port = master.port;
		record.deadline = now + ResolveTimeout;
		m_records.emplace_back(std::move(record));
	}
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::OnResolved(Query::EGame game, uint32_t generation, size_t index, std::expected<uint32_t, std::string> const& result, Clock::time_point now)
{
	size_t position{ 0 };

	for (SMasterRecord& record : m_records)
	{
		if (record.game == game)
		{
			if (record.generation == generation && position == index && record.state == EMasterState::Resolving)
			{
				if (result.has_value())
				{
					record.address = Query::SServerAddress{ result.value(), record.port };
					record.state = EMasterState::Querying;
					record.deadline = now;
				}
				else
				{
					record.failure = std::format("cannot be resolved: {}", result.error());
					record.state = EMasterState::Failed;
				}
			}

			++position;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::Update(Clock::time_point now, std::vector<SMasterQuery>& queries, std::vector<SMasterOutcome>& outcomes)
{
	for (SMasterRecord& record : m_records)
	{
		if (record.state == EMasterState::Resolving && record.deadline <= now)
		{
			record.failure = "could not be resolved in time";
			record.state = EMasterState::Failed;
		}

		if (record.state == EMasterState::Querying && record.deadline <= now)
		{
			if (record.numAttempts < MaxAttempts)
			{
				++record.numAttempts;
				record.deadline = now + ReplyTimeout;
				queries.emplace_back(record.game, record.address);
			}
			else
			{
				record.failure = "did not answer";
				record.state = EMasterState::Failed;
			}
		}

		if (record.state == EMasterState::Receiving && record.deadline <= now)
		{
			record.state = EMasterState::Done;
			outcomes.emplace_back(record.game, record.host, std::string{});
		}

		if (record.state == EMasterState::Failed)
		{
			record.state = EMasterState::Done;
			outcomes.emplace_back(record.game, record.host, record.failure);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Every listening record at that address hears it: two names of one game can resolve to the same master.
std::optional<Query::EGame> CMasterTracker::OnDatagram(Query::SServerAddress const& source, Clock::time_point now)
{
	std::optional<Query::EGame> game{};

	for (SMasterRecord& record : m_records)
	{
		if (IsListening(record) && record.address == source)
		{
			record.state = EMasterState::Receiving;
			record.deadline = now + QuietPeriod;
			game = record.game;
		}
	}

	return game;
}

//////////////////////////////////////////////////////////////////////////
size_t CMasterTracker::AdmitEntries(Query::SServerAddress const& source, size_t numEntries)
{
	size_t numAdmitted{ 0 };

	auto const it{ std::ranges::find_if(m_records, [&source](SMasterRecord const& record) { return IsListening(record) && record.address == source; }) };

	if (it != m_records.end())
	{
		numAdmitted = std::min(numEntries, MaxEntriesPerMaster - it->numEntries);
		it->numEntries += numAdmitted;
	}

	return numAdmitted;
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::Cancel(Query::EGame game)
{
	std::erase_if(m_records, [game](SMasterRecord const& record) { return record.game == game; });
}

//////////////////////////////////////////////////////////////////////////
bool CMasterTracker::HasWork(Query::EGame game) const
{
	return std::ranges::any_of(m_records, [game](SMasterRecord const& record) { return record.game == game && record.state != EMasterState::Done; });
}

//////////////////////////////////////////////////////////////////////////
std::optional<Clock::time_point> CMasterTracker::GetNextDeadline() const
{
	std::optional<Clock::time_point> deadline{};

	for (SMasterRecord const& record : m_records)
	{
		if (IsWaiting(record))
		{
			deadline = deadline.has_value() ? std::min(deadline.value(), record.deadline) : record.deadline;
		}
	}

	return deadline;
}
} // namespace Lkt::Net
