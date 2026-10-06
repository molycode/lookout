#include "protocol_note.hpp"
#include "query/protocol_definition.hpp"
#include "query/protocol_origin.hpp"
#include <format>

namespace Lkt::Ui
{
//////////////////////////////////////////////////////////////////////////
std::string DescribeProtocol(Query::SProtocolDefinition const& protocol)
{
	std::string text{};

	switch (protocol.origin)
	{
		case Query::EProtocolOrigin::Downloaded:
			text = protocol.downloadedVersion.has_value() ? std::format("Version {}, downloaded", *protocol.downloadedVersion) : std::string{ "Downloaded" };
			break;
		case Query::EProtocolOrigin::User:
			text = std::format("Your own, from protocols/{}.lua", protocol.name);
			break;
		case Query::EProtocolOrigin::UserOverDownloaded:
			text = protocol.downloadedVersion.has_value()
				? std::format("Your own, from protocols/{}.lua, in place of downloaded version {}", protocol.name, *protocol.downloadedVersion)
				: std::format("Your own, from protocols/{}.lua, in place of the downloaded one", protocol.name);
			break;
	}

	return text;
}
} // namespace Lkt::Ui
