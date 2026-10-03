#include "quake3_protocol.hpp"
#include "address_entry.hpp"
#include "bytes.hpp"
#include "status_body.hpp"
#include <string>

namespace Lkt::Query
{
namespace
{
constexpr std::string_view MasterQueryPrefix{ "\xFF\xFF\xFF\xFFgetservers " };
constexpr std::string_view MasterReplyHeader{ "\xFF\xFF\xFF\xFFgetserversResponse" };
constexpr std::string_view EndOfList{ "\\EOT" };
constexpr std::string_view StatusQuery{ "\xFF\xFF\xFF\xFFgetstatus" };
constexpr std::string_view StatusReplyHeader{ "\xFF\xFF\xFF\xFFstatusResponse\n" };

// Each entry is a backslash and then the address, so an address byte that happens to be a backslash is harmless.
constexpr size_t EntrySize{ 1 + AddressEntrySize };

//////////////////////////////////////////////////////////////////////////
// "\EOT" with nothing but NUL padding after it; a 69.79.84.x entry starts with the same four bytes.
bool IsEndOfList(std::string_view entries)
{
	return entries.starts_with(EndOfList) && entries.size() <= EntrySize
		&& entries.find_first_not_of('\0', EndOfList.size()) == std::string_view::npos;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> CQuake3Protocol::MasterRequest(SGameDefinition const& game) const
{
	std::string query{ MasterQueryPrefix };

	query += game.masterQueryArgs;

	return ToBytes(query);
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, EParseError> CQuake3Protocol::ParseMasterReply(std::span<std::byte const> datagram, std::vector<SServerAddress>& servers) const
{
	std::expected<void, EParseError> result{ std::unexpected{ EParseError::WrongHeader } };

	std::string_view const text{ AsText(datagram) };

	if (text.starts_with(MasterReplyHeader))
	{
		std::string_view entries{ text.substr(MasterReplyHeader.size()) };

		result = {};

		while (result.has_value() && !entries.empty())
		{
			if (IsEndOfList(entries))
			{
				entries = {};
			}
			else if (entries.front() != '\\')
			{
				result = std::unexpected{ EParseError::Malformed };
			}
			else if (entries.size() < EntrySize)
			{
				result = std::unexpected{ EParseError::Truncated };
			}
			else
			{
				servers.emplace_back(ReadAddressEntry(entries.substr(1, AddressEntrySize)));
				entries.remove_prefix(EntrySize);
			}
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> CQuake3Protocol::StatusRequest() const
{
	return ToBytes(StatusQuery);
}

//////////////////////////////////////////////////////////////////////////
std::expected<SStatusReply, EParseError> CQuake3Protocol::ParseStatusReply(std::span<std::byte const> datagram) const
{
	std::expected<SStatusReply, EParseError> result{ std::unexpected{ EParseError::WrongHeader } };

	std::string_view const text{ AsText(datagram) };

	if (text.starts_with(StatusReplyHeader))
	{
		result = ParseStatusBody(text.substr(StatusReplyHeader.size()));
	}

	return result;
}
} // namespace Lkt::Query
