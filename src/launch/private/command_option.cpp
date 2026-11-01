#include "launch/command_option.hpp"
#include "command_line.hpp"
#include "typed_command_syntax.hpp"
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
// A program named by its full path starts in its own folder, as a desktop shortcut does: old games find their data
// through the working directory. A relative path or a name from PATH keeps Lookout's.
std::expected<SLaunchOption, ELaunchError> MakeCommandOption(std::string_view command)
{
	std::expected<SLaunchOption, ELaunchError> result{ std::unexpected{ ELaunchError::EmptyCustomCommand } };
	std::expected<std::vector<std::string>, ECommandLineError> argv{ SplitCommandLine(command, GetTypedCommandSyntax(), EQuoting::Strict) };

	if (!argv.has_value())
	{
		result = std::unexpected{ ELaunchError::BadCustomCommand };
	}
	else if (!argv->empty() && !argv->front().empty())
	{
		std::filesystem::path const program{ argv->front() };
		std::string workingDir{ program.is_absolute() ? program.parent_path().string() : std::string{} };

		result = SLaunchOption{ {}, {}, {}, std::move(*argv), std::move(workingDir) };
	}

	return result;
}
} // namespace Lkt::Launch
