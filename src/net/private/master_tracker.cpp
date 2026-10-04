#include "master_tracker.hpp"
#include <algorithm>
#include <format>
#include <utility>

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
// A datagram from a master not yet asked answers nothing Lookout sent.
bool IsAsked(SMasterRecord const& record)
{
	return IsListening(record) && record.numAttempts > 0;
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

	for (size_t index{ 0 }; index < masters.size(); ++index)
	{
		Query::SMasterEndpoint const& master{ masters[index] };
		SMasterRecord record{};

		record.game = game;
		record.index = index;
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
	for (SMasterRecord& record : m_records)
	{
		if (record.game == game && record.generation == generation && record.index == index && record.state == EMasterState::Resolving)
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
				queries.emplace_back(SMasterId{ record.game, record.index }, record.address);
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
			outcomes.emplace_back(SMasterId{ record.game, record.index }, record.host, std::string{});
		}

		if (record.state == EMasterState::Failed)
		{
			record.state = EMasterState::Done;
			outcomes.emplace_back(SMasterId{ record.game, record.index }, record.host, record.failure);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Two names of one game can resolve to the same master, so every record there stays alive while it talks.
std::optional<SMasterId> CMasterTracker::OnDatagram(Query::SServerAddress const& source, Clock::time_point now)
{
	std::optional<SMasterId> master{};

	for (SMasterRecord& record : m_records)
	{
		if (IsAsked(record) && record.address == source)
		{
			record.state = EMasterState::Receiving;
			record.deadline = now + QuietPeriod;
			master = master.has_value() ? master : SMasterId{ record.game, record.index };
		}
	}

	return master;
}

//////////////////////////////////////////////////////////////////////////
size_t CMasterTracker::AdmitEntries(SMasterId const& master, size_t numEntries)
{
	size_t numAdmitted{ 0 };

	auto const it{ std::ranges::find_if(m_records, [&master](SMasterRecord const& record)
	{
		return IsAsked(record) && SMasterId{ record.game, record.index } == master;
	}) };

	if (it != m_records.end())
	{
		numAdmitted = std::min(numEntries, MaxEntriesPerMaster - it->numEntries);
		it->numEntries += numAdmitted;
	}

	return numAdmitted;
}

//////////////////////////////////////////////////////////////////////////
// Reported at once: a failed record arms no timer, so waiting for the next update could leave the refresh hanging.
void CMasterTracker::Fail(SMasterId const& master, std::string failure, std::vector<SMasterOutcome>& outcomes)
{
	auto const it{ std::ranges::find_if(m_records, [&master](SMasterRecord const& record)
	{
		return record.state != EMasterState::Done && SMasterId{ record.game, record.index } == master;
	}) };

	if (it != m_records.end())
	{
		it->state = EMasterState::Done;
		outcomes.emplace_back(master, it->host, std::move(failure));
	}
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
