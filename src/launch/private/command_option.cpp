#include "launch/command_option.hpp"
#include "command_line.hpp"
#include <string>
#include <utility>
#include <vector>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
std::expected<SLaunchOption, ELaunchError> MakeCommandOption(std::string_view command)
{
	std::expected<SLaunchOption, ELaunchError> result{ std::unexpected{ ELaunchError::EmptyCustomCommand } };
	std::expected<std::vector<std::string>, ECommandLineError> argv{ SplitCommandLine(command, EQuoting::Strict) };

	if (!argv.has_value())
	{
		result = std::unexpected{ ELaunchError::BadCustomCommand };
	}
	else if (!argv->empty() && !argv->front().empty())
	{
		result = SLaunchOption{ {}, {}, {}, std::move(*argv), {} };
	}

	return result;
}
} // namespace Lkt::Launch
