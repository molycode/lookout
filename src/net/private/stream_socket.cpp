#include "stream_socket.hpp"
#include "loggers.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace Lkt::Net
{
//////////////////////////////////////////////////////////////////////////
bool CStreamSocket::Initialize()
{
	m_descriptor = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

	if (m_descriptor < 0)
	{
		gLog.Error("Cannot create a TCP socket: {}", std::strerror(errno));
	}

	return m_descriptor >= 0;
}

//////////////////////////////////////////////////////////////////////////
void CStreamSocket::Terminate()
{
	if (m_descriptor >= 0)
	{
		close(m_descriptor);
		m_descriptor = -1;
	}
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, int> CStreamSocket::Connect(Query::SServerAddress const& peer) const
{
	sockaddr_in target{};

	target.sin_family = AF_INET;
	target.sin_port = htons(peer.port);
	target.sin_addr.s_addr = htonl(peer.ipv4);

	bool const isConnecting{ connect(m_descriptor, reinterpret_cast<sockaddr const*>(&target), sizeof(target)) == 0 || errno == EINPROGRESS };

	return isConnecting ? std::expected<void, int>{} : std::unexpected{ errno };
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, int> CStreamSocket::Send(std::span<std::byte const> bytes) const
{
	ssize_t const sent{ send(m_descriptor, bytes.data(), bytes.size(), MSG_NOSIGNAL) };
	std::expected<void, int> result{};

	if (sent < 0)
	{
		result = std::unexpected{ errno };
	}
	else if (static_cast<size_t>(sent) < bytes.size())
	{
		result = std::unexpected{ EWOULDBLOCK };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<size_t, int> CStreamSocket::Receive(std::span<std::byte> buffer) const
{
	ssize_t const received{ recv(m_descriptor, buffer.data(), buffer.size(), 0) };

	return (received >= 0) ? std::expected<size_t, int>{ static_cast<size_t>(received) } : std::unexpected{ errno };
}

//////////////////////////////////////////////////////////////////////////
int CStreamSocket::GetDescriptor() const
{
	return m_descriptor;
}
} // namespace Lkt::Net
