#include "net/https_fetcher.hpp"
#include "fetch_pump.hpp"
#include <tge/assert.hpp>
#include <utility>

namespace Lkt::Net
{
//////////////////////////////////////////////////////////////////////////
CHttpsFetcher::CHttpsFetcher() = default;

//////////////////////////////////////////////////////////////////////////
CHttpsFetcher::~CHttpsFetcher() = default;

//////////////////////////////////////////////////////////////////////////
bool CHttpsFetcher::Initialize(SHttpsOrigin origin, std::function<void()> onResults)
{
	TGE_ASSERT(m_pPump == nullptr, "Initialize twice");

	m_pPump = std::make_unique<CFetchPump>();

	bool const isReady{ m_pPump->Initialize(std::move(origin), std::move(onResults)) };

	if (!isReady)
	{
		m_pPump->Terminate();
		m_pPump.reset();
	}

	return isReady;
}

//////////////////////////////////////////////////////////////////////////
void CHttpsFetcher::Terminate()
{
	if (m_pPump != nullptr)
	{
		m_pPump->Terminate();
		m_pPump.reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CHttpsFetcher::Fetch(std::vector<SFetchRequest> requests)
{
	TGE_ASSERT(m_pPump != nullptr, "Fetch before Initialize");

	m_pPump->Fetch(std::move(requests));
}

//////////////////////////////////////////////////////////////////////////
void CHttpsFetcher::Cancel()
{
	TGE_ASSERT(m_pPump != nullptr, "Cancel before Initialize");

	m_pPump->Cancel();
}

//////////////////////////////////////////////////////////////////////////
void CHttpsFetcher::TakeResults(std::vector<SFetchResult>& results)
{
	TGE_ASSERT(m_pPump != nullptr, "TakeResults before Initialize");

	m_pPump->TakeResults(results);
}
} // namespace Lkt::Net
