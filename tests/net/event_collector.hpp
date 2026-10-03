#pragma once

#include "net/query_engine.hpp"
#include "net/query_event.hpp"
#include "query/game.hpp"
#include <tge/non_copyable.hpp>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <span>
#include <vector>

namespace Lkt::Fixtures
{
// Waits for a query engine the way the application does: on its callback, never by polling it.
class CEventCollector final : private Tge::SNoCopyNoMove
{
public:

	CEventCollector() = default;
	~CEventCollector() = default;

	std::function<void()> MakeCallback();

	bool WaitForFinish(Net::CQueryEngine& engine, Query::EGame game, std::chrono::milliseconds timeout);

	std::span<Net::SQueryEvent const> GetEvents() const;

private:

	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_hasEvents{ false };
	std::vector<Net::SQueryEvent> m_events;
};
} // namespace Lkt::Fixtures
