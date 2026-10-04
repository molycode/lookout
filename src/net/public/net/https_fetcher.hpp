#pragma once

#include "net/fetch_request.hpp"
#include "net/fetch_result.hpp"
#include "net/https_origin.hpp"
#include <tge/non_copyable.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace Lkt::Net
{
class CFetchPump;

// Fetches files from one HTTPS host on a tge event loop, one request at a time over a kept connection. Every call but
// Terminate returns at once; each request is answered by one result, announced through the callback given to
// Initialize, which runs on the loop's thread.
class CHttpsFetcher final : private Tge::SNoCopyNoMove
{
public:

	CHttpsFetcher();
	~CHttpsFetcher();

	bool Initialize(SHttpsOrigin origin, std::function<void()> onResults);
	void Terminate();

	void Fetch(std::vector<SFetchRequest> requests);
	// What has not been answered yet never will be.
	void Cancel();
	void TakeResults(std::vector<SFetchResult>& results);

private:

	std::unique_ptr<CFetchPump> m_pPump;
};
} // namespace Lkt::Net
