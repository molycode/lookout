#pragma once

#include "net/loopback_exchange.hpp"
#include "query/server_address.hpp"
#include <tge/non_copyable.hpp>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace Lkt::Fixtures
{
// A game server or master on 127.0.0.1 that answers each datagram with the replies of the first exchange it matches.
class CLoopbackServer final : private Tge::SNoCopyNoMove
{
public:

	CLoopbackServer() = default;
	~CLoopbackServer() = default;

	// The same reply to every request; none stays silent.
	bool Start(std::vector<std::byte> reply);
	bool StartExchanges(std::vector<SLoopbackExchange> exchanges);
	void Stop();

	Query::SServerAddress GetAddress() const;
	uint32_t GetNumRequests() const;
	// Read once stopped.
	std::vector<std::byte> GetFirstRequest() const;
	bool AreRequestsAlike() const;

private:

	static constexpr size_t MaxRequestSize{ 2048 };

	void Serve();

	int m_descriptor{ -1 };
	uint16_t m_port{ 0 };
	std::vector<SLoopbackExchange> m_exchanges;
	std::atomic<bool> m_isServing{ false };
	std::atomic<uint32_t> m_numRequests{ 0 };
	std::array<std::byte, MaxRequestSize> m_firstRequest{};
	size_t m_firstRequestSize{ 0 };
	std::atomic<bool> m_areRequestsAlike{ true };
	std::thread m_thread;
};
} // namespace Lkt::Fixtures
