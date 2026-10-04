#include "net/loopback_server.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <thread>
#include <unistd.h>
#include <utility>

namespace Lkt::Fixtures
{
namespace
{
constexpr uint32_t Loopback{ 0x7F000001 };
constexpr int PollIntervalMs{ 20 };
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CLoopbackServer::Start(std::vector<std::byte> reply)
{
	SLoopbackExchange exchange{};

	if (!reply.empty())
	{
		exchange.replies.emplace_back(std::move(reply));
	}

	return StartExchanges({ std::move(exchange) });
}

//////////////////////////////////////////////////////////////////////////
bool CLoopbackServer::StartExchanges(std::vector<SLoopbackExchange> exchanges)
{
	m_exchanges = std::move(exchanges);
	m_numMatches.assign(m_exchanges.size(), 0);
	m_descriptor = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);

	sockaddr_in local{};

	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(Loopback);

	socklen_t localSize{ sizeof(local) };
	bool const isBound{ m_descriptor >= 0 && bind(m_descriptor, reinterpret_cast<sockaddr const*>(&local), sizeof(local)) == 0
		&& getsockname(m_descriptor, reinterpret_cast<sockaddr*>(&local), &localSize) == 0 };

	if (isBound)
	{
		m_port = ntohs(local.sin_port);
		m_isServing.store(true, std::memory_order_release);
		m_thread = std::thread{ [this]() { Serve(); } };
	}

	return isBound;
}

//////////////////////////////////////////////////////////////////////////
void CLoopbackServer::Stop()
{
	m_isServing.store(false, std::memory_order_release);

	if (m_thread.joinable())
	{
		m_thread.join();
	}

	if (m_descriptor >= 0)
	{
		close(m_descriptor);
		m_descriptor = -1;
	}
}

//////////////////////////////////////////////////////////////////////////
Query::SServerAddress CLoopbackServer::GetAddress() const
{
	return Query::SServerAddress{ Loopback, m_port };
}

//////////////////////////////////////////////////////////////////////////
uint32_t CLoopbackServer::GetNumRequests() const
{
	return m_numRequests.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> CLoopbackServer::GetFirstRequest() const
{
	return std::vector<std::byte>{ m_firstRequest.begin(), m_firstRequest.begin() + static_cast<std::ptrdiff_t>(m_firstRequestSize) };
}

//////////////////////////////////////////////////////////////////////////
bool CLoopbackServer::AreRequestsAlike() const
{
	return m_areRequestsAlike.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
uint32_t CLoopbackServer::GetNumMatches(size_t exchange) const
{
	return m_numMatches[exchange];
}

//////////////////////////////////////////////////////////////////////////
// Allocates nothing, so it needs no thread setup for tge's allocator.
void CLoopbackServer::Serve()
{
	std::array<std::byte, MaxRequestSize> buffer{};

	while (m_isServing.load(std::memory_order_acquire))
	{
		pollfd descriptor{ m_descriptor, POLLIN, 0 };

		if (poll(&descriptor, 1, PollIntervalMs) > 0)
		{
			sockaddr_in sender{};
			socklen_t senderSize{ sizeof(sender) };

			ssize_t const size{ recvfrom(m_descriptor, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&sender), &senderSize) };

			if (size >= 0)
			{
				std::span<std::byte const> const request{ buffer.data(), static_cast<size_t>(size) };

				if (m_numRequests.load(std::memory_order_acquire) == 0)
				{
					std::ranges::copy(request, m_firstRequest.begin());
					m_firstRequestSize = request.size();
				}
				else if (!std::ranges::equal(request, std::span<std::byte const>{ m_firstRequest.data(), m_firstRequestSize }))
				{
					m_areRequestsAlike.store(false, std::memory_order_release);
				}

				m_numRequests.fetch_add(1, std::memory_order_acq_rel);

				auto const exchange{ std::ranges::find_if(m_exchanges, [request](SLoopbackExchange const& candidate)
				{
					return candidate.request.empty() || std::ranges::equal(candidate.request, request);
				}) };

				if (exchange != m_exchanges.end())
				{
					uint32_t& numMatches{ m_numMatches[static_cast<size_t>(exchange - m_exchanges.begin())] };

					++numMatches;

					if (numMatches > exchange->numIgnored)
					{
						for (std::vector<std::byte> const& reply : exchange->replies)
						{
							std::this_thread::sleep_for(exchange->replyInterval);
							sendto(m_descriptor, reply.data(), reply.size(), 0, reinterpret_cast<sockaddr const*>(&sender), senderSize);
						}
					}
				}
			}
		}
	}
}
} // namespace Lkt::Fixtures
