#include "net/socket_table.hpp"
#include <bit>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace Lkt::Fixtures
{
namespace
{
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
} // namespace

//////////////////////////////////////////////////////////////////////////
// The tables print an IPv4 address as the 32-bit word it is stored in, so a little-endian machine shows it reversed.
size_t CountSocketsTo(Query::SServerAddress const& peer)
{
	std::set<std::string> const inodes{ ListOwnSocketInodes() };
	std::string const remote{ std::format("{:08X}:{:04X}", std::byteswap(peer.ipv4), peer.port) };
	size_t numSockets{ 0 };

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

	return numSockets;
}
} // namespace Lkt::Fixtures
