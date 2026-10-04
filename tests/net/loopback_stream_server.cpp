#include "net/loopback_stream_server.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <span>
#include <unistd.h>
#include <utility>

namespace Lkt::Fixtures
{
namespace
{
constexpr uint32_t Loopback{ 0x7F000001 };
constexpr int PollIntervalMs{ 20 };
constexpr size_t MaxRequestSize{ 256 };
// Apart, so the client sees the reply arrive in pieces rather than as one segment.
constexpr std::chrono::milliseconds WriteInterval{ 1 };

//////////////////////////////////////////////////////////////////////////
bool SendAll(int connection, std::span<std::byte const> bytes)
{
	bool isSent{ true };

	while (isSent && !bytes.empty())
	{
		ssize_t const sent{ send(connection, bytes.data(), bytes.size(), MSG_NOSIGNAL) };

		isSent = sent > 0;
		bytes = isSent ? bytes.subspan(static_cast<size_t>(sent)) : bytes;
	}

	return isSent;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CLoopbackStreamServer::Start(SLoopbackStream stream)
{
	m_stream = std::move(stream);
	m_descriptor = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

	sockaddr_in local{};

	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(Loopback);

	socklen_t localSize{ sizeof(local) };
	bool const isListening{ m_descriptor >= 0 && bind(m_descriptor, reinterpret_cast<sockaddr const*>(&local), sizeof(local)) == 0
		&& listen(m_descriptor, 4) == 0 && getsockname(m_descriptor, reinterpret_cast<sockaddr*>(&local), &localSize) == 0 };

	if (isListening)
	{
		m_port = ntohs(local.sin_port);
		m_isServing.store(true, std::memory_order_release);
		m_thread = std::thread{ [this]() { Serve(); } };
	}

	return isListening;
}

//////////////////////////////////////////////////////////////////////////
void CLoopbackStreamServer::Stop()
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
		pollfd descriptor{ m_descriptor, POLLIN, 0 };

		if (poll(&descriptor, 1, PollIntervalMs) > 0)
		{
			int const connection{ accept4(m_descriptor, nullptr, nullptr, SOCK_CLOEXEC) };

			if (connection >= 0)
			{
				int const noDelay{ 1 };

				setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
				m_numConnections.fetch_add(1, std::memory_order_acq_rel);
				Converse(connection);
				close(connection);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Waits on the client between steps, so a client that closes, or Stop, ends the conversation.
void CLoopbackStreamServer::Converse(int connection)
{
	std::array<std::byte, MaxRequestSize> request{};
	size_t numReceived{ 0 };
	bool isOpen{ m_stream.greeting.empty() || SendAll(connection, m_stream.greeting) };
	bool hasRequest{ false };

	while (isOpen && m_isServing.load(std::memory_order_acquire))
	{
		pollfd descriptor{ connection, POLLIN, 0 };

		if (poll(&descriptor, 1, PollIntervalMs) > 0)
		{
			ssize_t const received{ recv(connection, request.data() + numReceived, request.size() - numReceived, 0) };

			isOpen = received > 0;
			numReceived += isOpen ? static_cast<size_t>(received) : 0;
		}

		if (isOpen && !hasRequest && !m_stream.request.empty() && numReceived >= m_stream.request.size())
		{
			hasRequest = true;
			isOpen = std::ranges::equal(std::span<std::byte const>{ request.data(), m_stream.request.size() }, m_stream.request);

			for (size_t start{ 0 }; isOpen && start < m_stream.reply.size(); start += m_stream.writeSize)
			{
				size_t const size{ std::min(m_stream.writeSize, m_stream.reply.size() - start) };

				isOpen = SendAll(connection, std::span<std::byte const>{ m_stream.reply }.subspan(start, size));
				std::this_thread::sleep_for(WriteInterval);
			}

			isOpen = isOpen && !m_stream.closesAfterReply;
		}
	}
}
} // namespace Lkt::Fixtures
