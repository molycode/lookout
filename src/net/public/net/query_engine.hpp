#pragma once

#include "net/query_event.hpp"
#include "query/game.hpp"
#include "query/server_address.hpp"
#include <tge/non_copyable.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace Lkt::Net
{
class CQueryPump;

// Queries masters and servers on a tge event loop. Every call but Terminate returns at once; results come back as
// events, announced through the callback given to Initialize, which runs on the loop's thread.
class CQueryEngine final : private Tge::SNoCopyNoMove
{
public:

	CQueryEngine();
	~CQueryEngine();

	bool Initialize(std::function<void()> onEventsReady);
	void Terminate();

	uint32_t Refresh(Query::EGame game, std::span<Query::SServerAddress const> favourites);
	void RefreshServer(Query::EGame game, Query::SServerAddress const& address);
	void Cancel(Query::EGame game);

	void TakeEvents(std::vector<SQueryEvent>& events);

private:

	std::unique_ptr<CQueryPump> m_pPump;
	uint32_t m_lastRefreshId{ 0 };
};
} // namespace Lkt::Net
