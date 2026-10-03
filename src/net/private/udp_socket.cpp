#include "udp_socket.hpp"
#include "loggers.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace Lkt::Net
{
namespace
{
// A master of a thousand servers and dozens of status replies can land at once; the default buffer drops some.
constexpr int ReceiveBufferSize{ 1024 * 1024 };
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CUdpSocket::Initialize()
{
	m_descriptor = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

	if (m_descriptor >= 0)
	{
		if (setsockopt(m_descriptor, SOL_SOCKET, SO_RCVBUF, &ReceiveBufferSize, sizeof(ReceiveBufferSize)) != 0)
		{
			gLog.Warning("Cannot enlarge the receive buffer, replies may be dropped under load: {}", std::strerror(errno));
		}
	}
	else
	{
		gLog.Error("Cannot create the UDP socket: {}", std::strerror(errno));
	}

	return m_descriptor >= 0;
}

//////////////////////////////////////////////////////////////////////////
void CUdpSocket::Terminate()
{
	if (m_descriptor >= 0)
	{
		close(m_descriptor);
		m_descriptor = -1;
	}
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, int> CUdpSocket::Send(Query::SServerAddress const& destination, std::span<std::byte const> datagram) const
{
	sockaddr_in target{};

	target.sin_family = AF_INET;
	target.sin_port = htons(destination.port);
	target.sin_addr.s_addr = htonl(destination.ipv4);

	ssize_t const sent{ sendto(m_descriptor, datagram.data(), datagram.size(), 0, reinterpret_cast<sockaddr const*>(&target), sizeof(target)) };

	return (sent >= 0) ? std::expected<void, int>{} : std::unexpected{ errno };
}

//////////////////////////////////////////////////////////////////////////
std::expected<size_t, int> CUdpSocket::Receive(std::span<std::byte> buffer, Query::SServerAddress& source) const
{
	sockaddr_in sender{};
	socklen_t senderSize{ sizeof(sender) };

	ssize_t const received{ recvfrom(m_descriptor, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&sender), &senderSize) };

	std::expected<size_t, int> result{ std::unexpected{ errno } };

	if (received >= 0)
	{
		source = Query::SServerAddress{ ntohl(sender.sin_addr.s_addr), ntohs(sender.sin_port) };
		result = static_cast<size_t>(received);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
int CUdpSocket::GetDescriptor() const
{
	return m_descriptor;
}
} // namespace Lkt::Net
