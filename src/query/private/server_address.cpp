#include "query/server_address.hpp"
#include <array>
#include <charconv>
#include <format>

namespace Lkt::Query
{
namespace
{
constexpr uint32_t NumOctets{ 4 };

//////////////////////////////////////////////////////////////////////////
// The whole text must be the number: from_chars on its own accepts "12abc" as 12.
template<typename T>
bool ParseWholeNumber(std::string_view text, T& value)
{
	char const* const pEnd{ text.data() + text.size() };
	std::from_chars_result const result{ std::from_chars(text.data(), pEnd, value) };

	return !text.empty() && result.ec == std::errc{} && result.ptr == pEnd;
}

//////////////////////////////////////////////////////////////////////////
bool ParseIpv4(std::string_view text, uint32_t& ipv4)
{
	bool valid{ true };
	uint32_t numParsed{ 0 };
	size_t start{ 0 };

	while (valid && numParsed < NumOctets)
	{
		size_t const dot{ text.find('.', start) };
		bool const isLast{ numParsed + 1 == NumOctets };
		uint8_t octet{ 0 };

		valid = (dot == std::string_view::npos) == isLast
			&& ParseWholeNumber(text.substr(start, isLast ? std::string_view::npos : dot - start), octet);

		ipv4 = (ipv4 << 8) | octet;
		start = dot + 1;
		++numParsed;
	}

	return valid;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<SServerAddress, EParseError> ParseAddress(std::string_view text)
{
	std::expected<SServerAddress, EParseError> result{ std::unexpected{ EParseError::Malformed } };

	size_t const colon{ text.rfind(':') };

	if (colon != std::string_view::npos)
	{
		SServerAddress address{};

		if (ParseIpv4(text.substr(0, colon), address.ipv4) && ParseWholeNumber(text.substr(colon + 1), address.port) && address.port != 0)
		{
			result = address;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
bool IsQueryable(SServerAddress const& address)
{
	uint32_t const ip{ address.ipv4 };
	uint32_t const firstOctet{ ip >> 24 };

	bool const isUnroutable{ firstOctet == 0 || firstOctet == 10 || firstOctet == 127 || firstOctet >= 224
		|| (ip & 0xFFF00000) == 0xAC100000
		|| (ip & 0xFFFF0000) == 0xC0A80000
		|| (ip & 0xFFFF0000) == 0xA9FE0000
		|| (ip & 0xFFC00000) == 0x64400000 };

	return !isUnroutable && address.port != 0;
}

//////////////////////////////////////////////////////////////////////////
std::string FormatAddress(SServerAddress const& address)
{
	std::array<char, MaxFormattedAddress> buffer{};

	return std::string{ FormatAddressTo(address, buffer) };
}

//////////////////////////////////////////////////////////////////////////
std::string_view FormatAddressTo(SServerAddress const& address, std::span<char, MaxFormattedAddress> buffer)
{
	auto const result{ std::format_to_n(buffer.data(), static_cast<std::ptrdiff_t>(buffer.size()), "{}.{}.{}.{}:{}",
		address.ipv4 >> 24, (address.ipv4 >> 16) & 0xFF, (address.ipv4 >> 8) & 0xFF, address.ipv4 & 0xFF, address.port) };

	return std::string_view{ buffer.data(), result.out };
}
} // namespace Lkt::Query
