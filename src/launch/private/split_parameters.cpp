#include "launch/split_parameters.hpp"
#include "command_line.hpp"
#include "typed_command_syntax.hpp"
#include <utility>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
// Split as a typed command is, so a parameter holding a space is quoted the same way there and here.
std::expected<std::vector<std::string>, ELaunchError> SplitParameters(std::string_view parameters)
{
	std::expected<std::vector<std::string>, ECommandLineError> split{ SplitCommandLine(parameters, GetTypedCommandSyntax(), EQuoting::Strict) };

	return split.has_value() ? std::expected<std::vector<std::string>, ELaunchError>{ std::move(*split) }
		: std::expected<std::vector<std::string>, ELaunchError>{ std::unexpected{ ELaunchError::BadParameters } };
}
} // namespace Lkt::Launch
