#include "net/loopback_stream_server.hpp"
#include <tge/net/ipv4_endpoint.hpp>
#include <tge/net/stream_socket.hpp>
#include <tge/net/wait_for_readable.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <expected>
#include <span>
#include <system_error>
#include <utility>

namespace Lkt::Fixtures
{
namespace
{
constexpr uint32_t Loopback{ 0x7F000001 };
constexpr std::chrono::milliseconds PollInterval{ 20 };
constexpr size_t MaxRequestSize{ 256 };
// Apart, so the client sees the reply arrive in pieces rather than as one segment.
constexpr std::chrono::milliseconds WriteInterval{ 1 };

//////////////////////////////////////////////////////////////////////////
bool SendAll(Tge::Net::CStreamSocket const& connection, std::span<std::byte const> bytes)
{
	bool isSent{ true };

	while (isSent && !bytes.empty())
	{
		size_t const sent{ connection.Send(bytes).value_or(0) };

		isSent = sent > 0;
		bytes = isSent ? bytes.subspan(sent) : bytes;
	}

	return isSent;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CLoopbackStreamServer::Start(SLoopbackStream stream)
{
	m_stream = std::move(stream);

	std::expected<Tge::Net::SIpv4Endpoint, std::error_code> const local{ m_listener.Initialize(Tge::Net::SIpv4Endpoint{ Loopback, 0 })
		.and_then([this]() { return m_listener.GetLocalEndpoint(); }) };

	if (local.has_value())
	{
		m_port = local->port;
		m_isServing.store(true, std::memory_order_release);
		m_thread = std::thread{ [this]() { Serve(); } };
	}

	return local.has_value();
}

//////////////////////////////////////////////////////////////////////////
void CLoopbackStreamServer::Stop()
{
	m_isServing.store(false, std::memory_order_release);

	if (m_thread.joinable())
	{
		m_thread.join();
	}

	m_listener.Terminate();
}

//////////////////////////////////////////////////////////////////////////
Query::SServerAddress CLoopbackStreamServer::GetAddress() const
{
	return Query::SServerAddress{ Loopback, m_port };
}

//////////////////////////////////////////////////////////////////////////
uint32_t CLoopbackStreamServer::GetNumConnections() const
{
	return m_numConnections.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
// Allocates nothing, so it needs no thread setup for tge's allocator.
void CLoopbackStreamServer::Serve()
{
	while (m_isServing.load(std::memory_order_acquire))
	{
		Tge::Net::CStreamSocket connection{};

		if (Tge::Net::WaitForReadable(m_listener.GetWaitable(), PollInterval).value_or(false) && m_listener.Accept(connection).value_or(false)
			&& connection.SetBlocking().has_value())
		{
			static_cast<void>(connection.SetNoDelay());
			m_numConnections.fetch_add(1, std::memory_order_acq_rel);
			Converse(connection);
		}

		connection.Terminate();
	}
}

//////////////////////////////////////////////////////////////////////////
// A step that expects nothing goes at once; one whose bytes differ ends the connection.
void CLoopbackStreamServer::Converse(Tge::Net::CStreamSocket const& connection)
{
	std::array<std::byte, MaxRequestSize> received{};
	size_t numReceived{ 0 };
	size_t step{ 0 };
	bool isOpen{ true };

	while (isOpen && m_isServing.load(std::memory_order_acquire))
	{
		std::vector<std::byte> const* const pExpect{ (step < m_stream.steps.size()) ? &m_stream.steps[step].expect : nullptr };

		if (pExpect != nullptr && numReceived >= pExpect->size())
		{
			std::span<std::byte const> const reply{ m_stream.steps[step].send };

			isOpen = std::ranges::equal(std::span<std::byte const>{ received.data(), pExpect->size() }, *pExpect);

			for (size_t start{ 0 }; isOpen && start < reply.size(); start += m_stream.writeSize)
			{
				isOpen = SendAll(connection, reply.subspan(start, std::min(m_stream.writeSize, reply.size() - start)));
				std::this_thread::sleep_for((start == 0) ? std::max(m_stream.steps[step].pause, WriteInterval) : WriteInterval);
			}

			numReceived = 0;
			++step;
			isOpen = isOpen && (step < m_stream.steps.size() || !m_stream.closesWhenDone);
		}
		else if (Tge::Net::WaitForReadable(connection.GetWaitable(), PollInterval).value_or(false))
		{
			size_t const offset{ (pExpect != nullptr) ? numReceived : 0 };
			size_t const wanted{ (pExpect != nullptr) ? pExpect->size() - numReceived : received.size() };
			size_t const count{ connection.Receive(std::span{ received }.subspan(offset, std::min(wanted, received.size() - offset))).value_or(0) };

			isOpen = count > 0;
			numReceived += (pExpect != nullptr) ? count : 0;
		}
	}
}
} // namespace Lkt::Fixtures
