#include "browser/browser_waiter.hpp"
#include "browser/browser.hpp"

namespace Lkt::Fixtures
{
//////////////////////////////////////////////////////////////////////////
std::function<void()> CBrowserWaiter::MakeCallback()
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
// The engine wakes once until its events are taken, so every wake is followed by an Update.
bool CBrowserWaiter::WaitUntil(Browser::CBrowser& browser, std::function<bool()> const& condition, std::chrono::milliseconds timeout)
{
	auto const deadline{ std::chrono::steady_clock::now() + timeout };
	bool isMet{ false };
	bool isWaiting{ true };

	while (!isMet && isWaiting)
	{
		browser.Update();
		isMet = condition();

		if (!isMet)
		{
			std::unique_lock lock{ m_mutex };

			isWaiting = m_wake.wait_until(lock, deadline, [this]() { return m_hasEvents; });
			m_hasEvents = false;
		}
	}

	return isMet;
}
} // namespace Lkt::Fixtures
