#include "browser/master_probes.hpp"
#include "net/query_catalog.hpp"
#include <tge/assert.hpp>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

namespace Lkt::Browser
{
namespace
{
// Long enough that a host or port being typed is not tested at every keystroke.
constexpr std::chrono::seconds SettleTime{ 1 };
constexpr Query::EGame ProbedGame{ 0 };
constexpr Query::EProtocol ProbedProtocol{ 0 };
constexpr std::string_view ProbedGameName{ "Master test" };
} // namespace

//////////////////////////////////////////////////////////////////////////
void CMasterProbes::Initialize(std::function<void()> wake)
{
	m_wake = std::move(wake);
}

//////////////////////////////////////////////////////////////////////////
void CMasterProbes::Terminate()
{
	StopRun();
	m_wanted.clear();
	m_wantedSince.clear();
	m_results.clear();
}

//////////////////////////////////////////////////////////////////////////
// A run goes on only while every row in it is still wanted and no other is due; otherwise it starts again with all
// that are due.
void CMasterProbes::Want(std::span<SMasterProbeRequest const> requests, std::span<Query::SProtocolDefinition const> protocols, bool canStart,
	Net::Clock::time_point now)
{
	std::map<std::string, Net::Clock::time_point, std::less<>> since{};

	for (SMasterProbeRequest const& request : requests)
	{
		auto const it{ m_wantedSince.find(request.key) };

		since.emplace(request.key, (it != m_wantedSince.end()) ? it->second : (m_isCleared ? now - SettleTime : now));
	}

	m_wanted.assign(requests.begin(), requests.end());
	m_wantedSince = std::move(since);
	m_lastNow = now;
	m_isCleared = false;

	auto const isDue{ [this](SMasterProbeRequest const& request) { return IsDue(request.key); } };
	bool const isRunStale{ m_isRunning && (std::ranges::any_of(m_runKeys, [this](std::string const& key) { return !m_wantedSince.contains(key); })
		|| std::ranges::any_of(m_wanted, isDue)) };

	if (isRunStale)
	{
		StopRun();
	}

	if (!m_isRunning && canStart && std::ranges::any_of(m_wanted, isDue))
	{
		StartRun(protocols);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMasterProbes::Update()
{
	bool isFinished{ false };

	if (m_isRunning)
	{
		m_engine.TakeEvents(m_events);
	}

	for (Net::SQueryEvent const& event : m_events)
	{
		std::visit([this, &isFinished](auto const& typed)
		{
			using TEvent = std::decay_t<decltype(typed)>;

			if constexpr (std::is_same_v<TEvent, Net::SMasterAnswered>)
			{
				m_results.insert_or_assign(m_runKeys[typed.index], SMasterProbeResult{ EProbeState::Answered, typed.numServers, {} });
			}
			else if constexpr (std::is_same_v<TEvent, Net::SMasterFailed>)
			{
				m_results.insert_or_assign(m_runKeys[typed.index], SMasterProbeResult{ EProbeState::Failed, 0, typed.reason });
			}
			else if constexpr (std::is_same_v<TEvent, Net::SRefreshFinished>)
			{
				isFinished = true;
			}
		}, event);
	}

	m_events.clear();

	if (isFinished)
	{
		StopRun();
	}
}

//////////////////////////////////////////////////////////////////////////
// A row with a result has settled, so without it the row is due again; one being tested is left to its run.
void CMasterProbes::Forget(std::string_view key)
{
	auto const result{ m_results.find(key) };

	if (result != m_results.end() && result->second.state != EProbeState::Probing)
	{
		m_results.erase(result);
	}
}

//////////////////////////////////////////////////////////////////////////
// The rows wanted next are tested at once, without waiting for them to settle.
void CMasterProbes::Clear()
{
	StopRun();
	m_wanted.clear();
	m_wantedSince.clear();
	m_results.clear();
	m_isCleared = true;
}

//////////////////////////////////////////////////////////////////////////
SMasterProbeResult const* CMasterProbes::GetResult(std::string_view key) const
{
	auto const it{ m_results.find(key) };

	return (it != m_results.end()) ? &it->second : nullptr;
}

//////////////////////////////////////////////////////////////////////////
// When a row being typed settles. One already due waits for the description to be usable, which only an edit brings.
std::optional<Net::Clock::time_point> CMasterProbes::GetNextDeadline() const
{
	std::optional<Net::Clock::time_point> next{};

	for (auto const& [key, since] : m_wantedSince)
	{
		Net::Clock::time_point const due{ since + SettleTime };

		if (!m_results.contains(key) && due > m_lastNow && (!next.has_value() || due < *next))
		{
			next = due;
		}
	}

	return next;
}

//////////////////////////////////////////////////////////////////////////
bool CMasterProbes::IsDue(std::string_view key) const
{
	auto const since{ m_wantedSince.find(key) };

	return since != m_wantedSince.end() && !m_results.contains(key) && since->second + SettleTime <= m_lastNow;
}

//////////////////////////////////////////////////////////////////////////
// An engine of its own, on a catalog of one game whose masters are the rows due and whose protocol is theirs, so
// starting it loads no other script.
void CMasterProbes::StartRun(std::span<Query::SProtocolDefinition const> protocols)
{
	Query::SGameDefinition game{};
	SMasterProbeRequest const* pFirst{ nullptr };

	for (SMasterProbeRequest const& request : m_wanted)
	{
		if (IsDue(request.key) && !std::ranges::contains(m_runKeys, request.key))
		{
			pFirst = (pFirst != nullptr) ? pFirst : &request;

			TGE_ASSERT(request.protocol == pFirst->protocol && request.protocolOptions == pFirst->protocolOptions, "The rows to test come from one description");

			game.masters.emplace_back(request.endpoint);
			m_runKeys.emplace_back(request.key);
		}
	}

	TGE_ASSERT(pFirst != nullptr && static_cast<size_t>(pFirst->protocol) < protocols.size(), "A run tests rows through a protocol it was given");

	game.name = ProbedGameName;
	game.game = ProbedGame;
	game.protocol = ProbedProtocol;
	game.protocolOptions = pFirst->protocolOptions;
	m_runProtocols.assign(1, protocols[static_cast<size_t>(pFirst->protocol)]);
	m_runGames.assign(1, std::move(game));
	m_isRunning = m_engine.Initialize(Net::SQueryCatalog{ m_runGames, m_runProtocols }, [wake = m_wake]() { wake(); });

	for (std::string const& key : m_runKeys)
	{
		m_results.insert_or_assign(key, m_isRunning ? SMasterProbeResult{}
			: SMasterProbeResult{ EProbeState::Failed, 0, "could not be tested; the log says why" });
	}

	if (m_isRunning)
	{
		m_engine.ProbeMasters(ProbedGame);
	}
	else
	{
		m_runKeys.clear();
		m_runGames.clear();
		m_runProtocols.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
// A row stopped while being tested is due again, if it is still wanted.
void CMasterProbes::StopRun()
{
	if (m_isRunning)
	{
		m_engine.Terminate();

		for (std::string const& key : m_runKeys)
		{
			auto const result{ m_results.find(key) };

			if (result != m_results.end() && result->second.state == EProbeState::Probing)
			{
				m_results.erase(result);
			}
		}

		m_runKeys.clear();
		m_runGames.clear();
		m_runProtocols.clear();
		m_events.clear();
		m_isRunning = false;
	}
}
} // namespace Lkt::Browser
