#include "quake2_protocol.hpp"
#include "address_entry.hpp"
#include "bytes.hpp"
#include "status_body.hpp"

namespace Lkt::Query
{
namespace
{
constexpr std::string_view MasterQuery{ "query" };
constexpr std::string_view MasterReplyHeader{ "\xFF\xFF\xFF\xFFservers" };
constexpr std::string_view StatusQuery{ "\xFF\xFF\xFF\xFFstatus\n" };
constexpr std::string_view StatusReplyHeader{ "\xFF\xFF\xFF\xFFprint\n" };
} // namespace

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> CQuake2Protocol::MasterRequest(SGameDefinition const& game) const
{
	return ToBytes(MasterQuery);
}

//////////////////////////////////////////////////////////////////////////
// Kingpin's master separates the header from the list with a newline, the Quake 2 masters with a space.
std::expected<void, EParseError> CQuake2Protocol::ParseMasterReply(std::span<std::byte const> datagram, std::vector<SServerAddress>& servers) const
{
	std::expected<void, EParseError> result{ std::unexpected{ EParseError::WrongHeader } };

	std::string_view const text{ AsText(datagram) };
	bool const hasHeader{ text.size() > MasterReplyHeader.size() && text.starts_with(MasterReplyHeader)
		&& (text[MasterReplyHeader.size()] == '\n' || text[MasterReplyHeader.size()] == ' ') };

	if (hasHeader)
	{
		std::string_view const entries{ text.substr(MasterReplyHeader.size() + 1) };
		size_t const numEntries{ entries.size() / AddressEntrySize };

		for (size_t index{ 0 }; index < numEntries; ++index)
		{
			servers.emplace_back(ReadAddressEntry(entries.substr(index * AddressEntrySize, AddressEntrySize)));
		}

		result = (entries.size() % AddressEntrySize == 0) ? std::expected<void, EParseError>{} : std::unexpected{ EParseError::Truncated };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> CQuake2Protocol::StatusRequest() const
{
	return ToBytes(StatusQuery);
}

//////////////////////////////////////////////////////////////////////////
std::expected<SStatusReply, EParseError> CQuake2Protocol::ParseStatusReply(std::span<std::byte const> datagram) const
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
