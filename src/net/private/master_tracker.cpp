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
constexpr Clock::duration StepTimeout{ std::chrono::seconds{ 2 } };
constexpr Clock::duration Deadline{ std::chrono::seconds{ 30 } };

// The busiest master measured lists about 1,000 servers; anything far beyond that is broken or hostile.
constexpr size_t MaxEntriesPerMaster{ 4096 };

//////////////////////////////////////////////////////////////////////////
bool IsAsked(SMasterRecord const& record)
{
	return record.state == EMasterState::Querying && record.numAttempts > 0;
}

//////////////////////////////////////////////////////////////////////////
bool IsStepExhausted(SMasterRecord const& record, Clock::time_point now)
{
	return !record.isStepAnswered && record.numAttempts >= MaxAttempts && now - record.stepSentAt >= StepTimeout;
}

//////////////////////////////////////////////////////////////////////////
// Empty when the list is complete, the failure when it was cut short; none while the conversation goes on.
std::optional<std::string> FindEnding(SMasterRecord const& record, Clock::time_point now)
{
	std::optional<std::string> ending{};

	if (now - record.startedAt >= Deadline)
	{
		ending = record.hasAnswered ? std::format("did not finish its list in {} s, after {} servers", Deadline / std::chrono::seconds{ 1 }, record.numEntries)
			: std::string{ "did not answer" };
	}
	else if (record.quiet.has_value() && now - record.lastDatagramAt >= *record.quiet)
	{
		ending = std::string{};
	}
	else if (IsStepExhausted(record, now))
	{
		ending = record.hasAnswered ? std::format("stopped answering after listing {} servers", record.numEntries) : std::string{ "did not answer" };
	}

	return ending;
}

//////////////////////////////////////////////////////////////////////////
std::optional<Clock::time_point> GetDeadline(SMasterRecord const& record)
{
	std::optional<Clock::time_point> deadline{};

	if (record.state == EMasterState::Resolving)
	{
		deadline = record.resolveDeadline;
	}
	else if (record.state == EMasterState::Querying && record.numAttempts == 0)
	{
		deadline = record.stepSentAt;
	}
	else if (record.state == EMasterState::Querying)
	{
		deadline = record.startedAt + Deadline;

		if (record.quiet.has_value())
		{
			deadline = std::min(*deadline, record.lastDatagramAt + *record.quiet);
		}

		if (!record.isStepAnswered)
		{
			deadline = std::min(*deadline, record.stepSentAt + StepTimeout);
		}
	}

	return deadline;
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
		record.resolveDeadline = now + ResolveTimeout;
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
				record.stepSentAt = now;
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
		if (record.state == EMasterState::Resolving && record.resolveDeadline <= now)
		{
			record.failure = "could not be resolved in time";
			record.state = EMasterState::Failed;
		}

		if (record.state == EMasterState::Querying && record.numAttempts == 0)
		{
			record.numAttempts = 1;
			record.startedAt = now;
			record.stepSentAt = now;
			queries.emplace_back(SMasterId{ record.game, record.index }, record.address);
		}
		else if (std::optional<std::string> ending{ IsAsked(record) ? FindEnding(record, now) : std::nullopt }; ending.has_value())
		{
			End(record, std::move(*ending), outcomes);
		}
		else if (IsAsked(record) && !record.isStepAnswered && record.numAttempts < MaxAttempts && now - record.stepSentAt >= StepTimeout)
		{
			++record.numAttempts;
			record.stepSentAt = now;
			queries.emplace_back(SMasterId{ record.game, record.index }, record.address);
		}

		if (record.state == EMasterState::Failed)
		{
			End(record, record.failure, outcomes);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::OnDatagram(SMasterId const& master, Clock::time_point now)
{
	SMasterRecord* const pRecord{ Find(master) };

	if (pRecord != nullptr && IsAsked(*pRecord))
	{
		pRecord->hasAnswered = true;
		pRecord->lastDatagramAt = now;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::MarkStepAnswered(SMasterId const& master)
{
	SMasterRecord* const pRecord{ Find(master) };

	if (pRecord != nullptr)
	{
		pRecord->isStepAnswered = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::BeginStep(SMasterId const& master, Clock::time_point now)
{
	SMasterRecord* const pRecord{ Find(master) };

	if (pRecord != nullptr)
	{
		pRecord->numAttempts = 1;
		pRecord->stepSentAt = now;
		pRecord->isStepAnswered = false;
		pRecord->quiet.reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::SetQuiet(SMasterId const& master, Clock::duration quiet)
{
	SMasterRecord* const pRecord{ Find(master) };

	if (pRecord != nullptr)
	{
		pRecord->quiet = quiet;
	}
}

//////////////////////////////////////////////////////////////////////////
size_t CMasterTracker::AdmitEntries(SMasterId const& master, size_t numEntries)
{
	SMasterRecord* const pRecord{ Find(master) };
	size_t numAdmitted{ 0 };

	if (pRecord != nullptr && IsAsked(*pRecord))
	{
		numAdmitted = std::min(numEntries, MaxEntriesPerMaster - pRecord->numEntries);
		pRecord->numEntries += numAdmitted;
	}

	return numAdmitted;
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::Finish(SMasterId const& master, std::vector<SMasterOutcome>& outcomes)
{
	SMasterRecord* const pRecord{ Find(master) };

	if (pRecord != nullptr && pRecord->state != EMasterState::Done)
	{
		End(*pRecord, std::string{}, outcomes);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::Fail(SMasterId const& master, std::string failure, std::vector<SMasterOutcome>& outcomes)
{
	SMasterRecord* const pRecord{ Find(master) };

	if (pRecord != nullptr && pRecord->state != EMasterState::Done)
	{
		End(*pRecord, std::move(failure), outcomes);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::Cancel(Query::EGame game)
{
	std::erase_if(m_records, [game](SMasterRecord const& record) { return record.game == game; });
}

//////////////////////////////////////////////////////////////////////////
bool CMasterTracker::IsAsking(SMasterId const& master) const
{
	return std::ranges::any_of(m_records, [&master](SMasterRecord const& record)
	{
		return IsAsked(record) && SMasterId{ record.game, record.index } == master;
	});
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
		std::optional<Clock::time_point> const recordDeadline{ GetDeadline(record) };

		if (recordDeadline.has_value())
		{
			deadline = deadline.has_value() ? std::min(*deadline, *recordDeadline) : recordDeadline;
		}
	}

	return deadline;
}

//////////////////////////////////////////////////////////////////////////
SMasterRecord* CMasterTracker::Find(SMasterId const& master)
{
	auto const it{ std::ranges::find_if(m_records, [&master](SMasterRecord const& record) { return SMasterId{ record.game, record.index } == master; }) };

	return (it != m_records.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CMasterTracker::End(SMasterRecord& record, std::string failure, std::vector<SMasterOutcome>& outcomes)
{
	record.state = EMasterState::Done;
	outcomes.emplace_back(SMasterId{ record.game, record.index }, record.host, std::move(failure));
}
} // namespace Lkt::Net
