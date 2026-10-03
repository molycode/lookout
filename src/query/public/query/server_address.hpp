#pragma once

#include "query/parse_error.hpp"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Query
{
// Host byte order.
struct SServerAddress final
{
	uint32_t ipv4{ 0 };
	uint16_t port{ 0 };
};

std::expected<SServerAddress, EParseError> ParseAddress(std::string_view text);
// "255.255.255.255:65535".
inline constexpr size_t MaxFormattedAddress{ 21 };

std::string FormatAddress(SServerAddress const& address);
// Without allocating, for text drawn every frame.
std::string_view FormatAddressTo(SServerAddress const& address, std::span<char, MaxFormattedAddress> buffer);

// False for what no public game server can be: a master listing loopback, private, link-local, multicast or
// broadcast addresses would otherwise make the browser probe the user's own network.
bool IsQueryable(SServerAddress const& address);

constexpr uint64_t ToKey(SServerAddress const& address)
{
	return (static_cast<uint64_t>(address.ipv4) << 16) | static_cast<uint64_t>(address.port);
}

constexpr SServerAddress FromKey(uint64_t key)
{
	return SServerAddress{ static_cast<uint32_t>(key >> 16), static_cast<uint16_t>(key & 0xFFFF) };
}

constexpr bool operator==(SServerAddress const& lhs, SServerAddress const& rhs)
{
	return lhs.ipv4 == rhs.ipv4 && lhs.port == rhs.port;
}
} // namespace Lkt::Query
