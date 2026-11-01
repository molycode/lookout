#include "net/socket_table.hpp"
#include "net/loopback_server.hpp"
#include <tge/platform.hpp>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(TGE_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <algorithm>
#include <span>
#elif defined(TGE_PLATFORM_LINUX)
#include <filesystem>
#include <format>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)

namespace Lkt::Fixtures
{
namespace
{
#if defined(TGE_PLATFORM_WINDOWS)
//////////////////////////////////////////////////////////////////////////
// Empty when the table cannot be read; it can grow between asking its size and reading it.
template<typename TRead>
std::vector<std::byte> ReadTable(TRead read)
{
	std::vector<std::byte> table{};
	DWORD size{ 0 };
	DWORD result{ read(nullptr, &size) };

	while (result == ERROR_INSUFFICIENT_BUFFER)
	{
		table.resize(size);
		result = read(table.data(), &size);
	}

	return (result == NO_ERROR) ? table : std::vector<std::byte>{};
}

//////////////////////////////////////////////////////////////////////////
// The tables hold addresses and ports in network order.
uint16_t ToHostPort(DWORD port)
{
	return std::byteswap(static_cast<uint16_t>(port));
}
#elif defined(TGE_PLATFORM_LINUX)
constexpr std::string_view SocketPrefix{ "socket:[" };
constexpr size_t RemoteAddressField{ 2 };
constexpr size_t InodeField{ 9 };

//////////////////////////////////////////////////////////////////////////
std::set<std::string> ListOwnSocketInodes()
{
	std::set<std::string> inodes{};
	std::error_code error{};

	for (std::filesystem::directory_iterator it{ "/proc/self/fd", error }, end{}; error.value() == 0 && it != end; it.increment(error))
	{
		std::string const target{ std::filesystem::read_symlink(it->path(), error).string() };

		if (target.starts_with(SocketPrefix))
		{
			inodes.emplace(target.substr(SocketPrefix.size(), target.size() - SocketPrefix.size() - 1));
		}
	}

	return inodes;
}
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
} // namespace

//////////////////////////////////////////////////////////////////////////
// Matched by peer in the system's tables, so sockets opened meanwhile by anything else, such as a DNS lookup glibc
// finishes, cannot disturb the count. Linux's tables print an IPv4 address as the word it is stored in, reversed on a
// little-endian machine; Windows' UDP table has no remote address at all, hence the overload for a UDP server.
size_t CountSocketsTo(Query::SServerAddress const& peer)
{
	size_t numSockets{ 0 };

#if defined(TGE_PLATFORM_WINDOWS)
	std::vector<std::byte> const table{ ReadTable([](void* pTable, DWORD* pSize)
	{
		return GetExtendedTcpTable(pTable, pSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
	}) };

	if (!table.empty())
	{
		MIB_TCPTABLE_OWNER_PID const* const pTable{ reinterpret_cast<MIB_TCPTABLE_OWNER_PID const*>(table.data()) };

		for (MIB_TCPROW_OWNER_PID const& row : std::span{ pTable->table, pTable->dwNumEntries })
		{
			bool const isOwn{ row.dwOwningPid == GetCurrentProcessId() };

			numSockets += (isOwn && std::byteswap(static_cast<uint32_t>(row.dwRemoteAddr)) == peer.ipv4 && ToHostPort(row.dwRemotePort) == peer.port) ? 1 : 0;
		}
	}
#elif defined(TGE_PLATFORM_LINUX)
	std::set<std::string> const inodes{ ListOwnSocketInodes() };
	std::string const remote{ std::format("{:08X}:{:04X}", std::byteswap(peer.ipv4), peer.port) };

	for (char const* const pTable : { "/proc/net/tcp", "/proc/net/udp" })
	{
		std::ifstream table{ pTable };
		std::string line{};

		std::getline(table, line);

		while (std::getline(table, line))
		{
			std::istringstream fields{ line };
			std::string field{};
			std::string remoteField{};
			std::string inodeField{};

			for (size_t index{ 0 }; fields >> field; ++index)
			{
				remoteField = (index == RemoteAddressField) ? field : remoteField;
				inodeField = (index == InodeField) ? field : inodeField;
			}

			numSockets += (remoteField == remote && inodes.contains(inodeField)) ? 1 : 0;
		}
	}
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)

	return numSockets;
}

//////////////////////////////////////////////////////////////////////////
// Windows: this process's UDP sockets on the ports the server heard from.
size_t CountSocketsTo(CLoopbackServer const& server)
{
#if defined(TGE_PLATFORM_WINDOWS)
	std::vector<uint16_t> const ports{ server.GetClientPorts() };
	std::vector<std::byte> const table{ ReadTable([](void* pTable, DWORD* pSize)
	{
		return GetExtendedUdpTable(pTable, pSize, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
	}) };
	size_t numSockets{ 0 };

	if (!table.empty())
	{
		MIB_UDPTABLE_OWNER_PID const* const pTable{ reinterpret_cast<MIB_UDPTABLE_OWNER_PID const*>(table.data()) };

		for (MIB_UDPROW_OWNER_PID const& row : std::span{ pTable->table, pTable->dwNumEntries })
		{
			bool const isOwn{ row.dwOwningPid == GetCurrentProcessId() };

			numSockets += (isOwn && std::ranges::contains(ports, ToHostPort(row.dwLocalPort))) ? 1 : 0;
		}
	}

	return numSockets;
#elif defined(TGE_PLATFORM_LINUX)
	return CountSocketsTo(server.GetAddress());
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
}
} // namespace Lkt::Fixtures
