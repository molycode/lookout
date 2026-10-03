#include "join_message.hpp"
#include <format>

namespace Lkt::Ui
{
//////////////////////////////////////////////////////////////////////////
std::string DescribeJoin(std::string_view gameName, Query::SServerAddress const& address, std::expected<Launch::SLaunchOption, Launch::ELaunchError> const& launcher,
	std::expected<void, Launch::ELaunchError> const& result)
{
	std::string text{};

	if (!result.has_value() && launcher.has_value())
	{
		text = std::format("Cannot start {} · {}: {}", launcher->name, launcher->location, Launch::ToString(result.error()));
	}
	else if (!result.has_value())
	{
		text = std::format("Cannot start {}: {}", gameName, Launch::ToString(result.error()));
	}
	else if (launcher.has_value())
	{
		text = std::format("Starting {} · {} for {}…", launcher->name, launcher->location, Query::FormatAddress(address));
	}
	else
	{
		text = std::format("Starting {} for {}…", gameName, Query::FormatAddress(address));
	}

	return text;
}
} // namespace Lkt::Ui
