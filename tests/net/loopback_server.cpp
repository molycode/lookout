#include "net/loopback_server.hpp"
#include <tge/net/ipv4_endpoint.hpp>
#include <tge/net/wait_for_readable.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <span>
#include <system_error>
#include <thread>
#include <utility>

namespace Lkt::Fixtures
{
namespace
{
constexpr uint32_t Loopback{ 0x7F000001 };
constexpr std::chrono::milliseconds PollInterval{ 20 };
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

	std::expected<Tge::Net::SIpv4Endpoint, std::error_code> const local{ m_socket.Initialize(Tge::Net::SIpv4Endpoint{ Loopback, 0 })
		.and_then([this]() { return m_socket.GetLocalEndpoint(); }) };

	if (local.has_value())
	{
		m_port = local->port;
		m_isServing.store(true, std::memory_order_release);
		m_thread = std::thread{ [this]() { Serve(); } };
	}

	return local.has_value();
}

//////////////////////////////////////////////////////////////////////////
void CLoopbackServer::Stop()
{
	m_isServing.store(false, std::memory_order_release);

	if (m_thread.joinable())
	{
		m_thread.join();
	}

	m_socket.Terminate();
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
std::vector<uint16_t> CLoopbackServer::GetClientPorts() const
{
	std::vector<uint16_t> ports{};
	size_t const numPorts{ m_numClientPorts.load(std::memory_order_acquire) };

	for (size_t index{ 0 }; index < numPorts; ++index)
	{
		ports.emplace_back(m_clientPorts[index].load(std::memory_order_relaxed));
	}

	return ports;
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
		if (Tge::Net::WaitForReadable(m_socket.GetWaitable(), PollInterval).value_or(false))
		{
			Tge::Net::SIpv4Endpoint sender{};
			std::expected<size_t, std::error_code> const size{ m_socket.Receive(buffer, sender) };

			if (size.has_value())
			{
				std::span<std::byte const> const request{ buffer.data(), *size };

				Record(request, sender.port);

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
						for (size_t index{ 0 }; index < exchange->replies.size(); ++index)
						{
							std::this_thread::sleep_for(exchange->replyInterval);

							if (index == 0 && exchange->isDroppingQueued)
							{
								DropQueued(buffer);
							}

							static_cast<void>(m_socket.Send(sender, exchange->replies[index]));
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CLoopbackServer::Record(std::span<std::byte const> request, uint16_t port)
{
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
	RecordClientPort(port);
}

//////////////////////////////////////////////////////////////////////////
// Only a request sent without waiting for the answer can be queued yet, as the answer is not sent until after this.
void CLoopbackServer::DropQueued(std::span<std::byte> buffer)
{
	while (Tge::Net::WaitForReadable(m_socket.GetWaitable(), std::chrono::milliseconds{ 0 }).value_or(false))
	{
		Tge::Net::SIpv4Endpoint sender{};
		std::expected<size_t, std::error_code> const size{ m_socket.Receive(buffer, sender) };

		if (size.has_value())
		{
			Record(std::span<std::byte const>{ buffer.data(), *size }, sender.port);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Written by the serving thread alone, and into fixed storage, as it may not allocate.
void CLoopbackServer::RecordClientPort(uint16_t port)
{
	size_t const numPorts{ m_numClientPorts.load(std::memory_order_acquire) };
	bool const isKnown{ std::ranges::any_of(std::span{ m_clientPorts }.first(numPorts), [port](std::atomic<uint16_t> const& known)
	{
		return known.load(std::memory_order_relaxed) == port;
	}) };

	if (!isKnown && numPorts < m_clientPorts.size())
	{
		m_clientPorts[numPorts].store(port, std::memory_order_relaxed);
		m_numClientPorts.store(numPorts + 1, std::memory_order_release);
	}
}
} // namespace Lkt::Fixtures
