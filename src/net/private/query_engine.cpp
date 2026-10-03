#include "net/query_engine.hpp"
#include "query_pump.hpp"
#include <tge/assert.hpp>
#include <utility>

namespace Lkt::Net
{
//////////////////////////////////////////////////////////////////////////
CQueryEngine::CQueryEngine() = default;

//////////////////////////////////////////////////////////////////////////
CQueryEngine::~CQueryEngine() = default;

//////////////////////////////////////////////////////////////////////////
bool CQueryEngine::Initialize(std::function<void()> onEventsReady)
{
	TGE_ASSERT(m_pPump == nullptr, "Initialize twice");

	m_pPump = std::make_unique<CQueryPump>();

	bool const isReady{ m_pPump->Initialize(std::move(onEventsReady)) };

	if (!isReady)
	{
		m_pPump->Terminate();
		m_pPump.reset();
	}

	return isReady;
}

//////////////////////////////////////////////////////////////////////////
void CQueryEngine::Terminate()
{
	if (m_pPump != nullptr)
	{
		m_pPump->Terminate();
		m_pPump.reset();
	}
}

//////////////////////////////////////////////////////////////////////////
uint32_t CQueryEngine::Refresh(Query::EGame game, std::span<Query::SServerAddress const> favourites)
{
	TGE_ASSERT(m_pPump != nullptr, "Refresh before Initialize");

	++m_lastRefreshId;
	m_pPump->Refresh(game, { favourites.begin(), favourites.end() }, m_lastRefreshId);

	return m_lastRefreshId;
}

//////////////////////////////////////////////////////////////////////////
void CQueryEngine::RefreshServer(Query::EGame game, Query::SServerAddress const& address)
{
	TGE_ASSERT(m_pPump != nullptr, "RefreshServer before Initialize");

	++m_lastRefreshId;
	m_pPump->RefreshServer(game, address, m_lastRefreshId);
}

//////////////////////////////////////////////////////////////////////////
void CQueryEngine::Cancel(Query::EGame game)
{
	TGE_ASSERT(m_pPump != nullptr, "Cancel before Initialize");

	m_pPump->Cancel(game);
}

//////////////////////////////////////////////////////////////////////////
void CQueryEngine::TakeEvents(std::vector<SQueryEvent>& events)
{
	TGE_ASSERT(m_pPump != nullptr, "TakeEvents before Initialize");

	m_pPump->TakeEvents(events);
}
} // namespace Lkt::Net
