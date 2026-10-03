#pragma once

#include <tge/non_copyable.hpp>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>

namespace Lkt
{
namespace Browser
{
class CBrowser;
} // namespace Browser

namespace Fixtures
{
// Drives a browser the way the window does: Update after every wake, never by polling.
class CBrowserWaiter final : private Tge::SNoCopyNoMove
{
public:

	CBrowserWaiter() = default;
	~CBrowserWaiter() = default;

	std::function<void()> MakeCallback();

	bool WaitUntil(Browser::CBrowser& browser, std::function<bool()> const& condition, std::chrono::milliseconds timeout);

private:

	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_hasEvents{ false };
};
} // namespace Fixtures
} // namespace Lkt
