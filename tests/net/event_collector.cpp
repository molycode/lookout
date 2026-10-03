#include "net/event_collector.hpp"
#include <algorithm>

namespace Lkt::Fixtures
{
//////////////////////////////////////////////////////////////////////////
std::function<void()> CEventCollector::MakeCallback()
{
	return [this]()
	{
		{
			std::lock_guard const lock{ m_mutex };

			m_hasEvents = true;
		}

		m_wake.notify_one();
	};
}

//////////////////////////////////////////////////////////////////////////
bool CEventCollector::WaitForFinish(Net::CQueryEngine& engine, Query::EGame game, std::chrono::milliseconds timeout)
{
	auto const deadline{ std::chrono::steady_clock::now() + timeout };
	auto const isFinish{ [game](Net::SQueryEvent const& event)
	{
		Net::SRefreshFinished const* const pFinished{ std::get_if<Net::SRefreshFinished>(&event) };

		return pFinished != nullptr && pFinished->game == game;
	} };

	bool isFinished{ false };
	bool isWaiting{ true };

	while (!isFinished && isWaiting)
	{
		{
			std::unique_lock lock{ m_mutex };

			isWaiting = m_wake.wait_until(lock, deadline, [this]() { return m_hasEvents; });
			m_hasEvents = false;
		}

		size_t const numBefore{ m_events.size() };

		engine.TakeEvents(m_events);
		isFinished = std::any_of(m_events.begin() + static_cast<std::ptrdiff_t>(numBefore), m_events.end(), isFinish);
	}

	return isFinished;
}

//////////////////////////////////////////////////////////////////////////
std::span<Net::SQueryEvent const> CEventCollector::GetEvents() const
{
	return m_events;
}
} // namespace Lkt::Fixtures
