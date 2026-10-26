#pragma once

#include <format>
#include <string>
#include <string_view>

namespace Lkt::Ui
{
inline std::string DescribeMasterListing(std::string_view inheritedProtocol, std::string_view protocol, std::string_view portOffset)
{
	std::string text{ !protocol.empty() ? std::string{ protocol }
		: (inheritedProtocol.empty() ? std::string{ "The game's" } : std::format("{} (the game's)", inheritedProtocol)) };

	if (!portOffset.empty())
	{
		text += std::format(" · {}{}", (portOffset.starts_with('-') || portOffset.starts_with('+')) ? "" : "+", portOffset);
	}

	return text;
}
} // namespace Lkt::Ui
