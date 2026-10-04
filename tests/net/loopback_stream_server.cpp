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
// A step that expects nothing goes at once; one whose bytes differ ends the connection.
void CLoopbackStreamServer::Converse(int connection)
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
		else if (pollfd descriptor{ connection, POLLIN, 0 }; poll(&descriptor, 1, PollIntervalMs) > 0)
		{
			size_t const offset{ (pExpect != nullptr) ? numReceived : 0 };
			size_t const wanted{ (pExpect != nullptr) ? pExpect->size() - numReceived : received.size() };
			ssize_t const count{ recv(connection, received.data() + offset, std::min(wanted, received.size() - offset), 0) };

			isOpen = count > 0;
			numReceived += (isOpen && pExpect != nullptr) ? static_cast<size_t>(count) : 0;
		}
	}
}
} // namespace Lkt::Fixtures
