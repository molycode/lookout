#include "install_folder.hpp"
#include "program_path.hpp"
#include <algorithm>
#include <format>
#include <system_error>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
std::expected<std::filesystem::path, std::string> CheckInstallFolder(SLaunchHints const& hints, std::filesystem::path const& folder)
{
	std::expected<std::filesystem::path, std::string> result{ std::unexpected{ std::string{ "it is not a folder" } } };
	std::error_code error{};

	if (std::filesystem::is_directory(folder, error))
	{
		std::filesystem::path const program{ folder / hints.program };
		std::expected<void, EProgramError> const check{ CheckExecutable(program) };
		auto const missing{ std::ranges::find_if(hints.requiredFiles, [&folder](std::string_view file)
		{
			std::error_code existsError{};

			return !std::filesystem::exists(folder / file, existsError);
		}) };

		if (missing != hints.requiredFiles.end())
		{
			result = std::unexpected{ std::format("'{}' is missing", *missing) };
		}
		else if (!check.has_value())
		{
			result = std::unexpected{ std::format("'{}' {}", hints.program, ToString(check.error())) };
		}
		else
		{
			result = program;
		}
	}

	return result;
}
} // namespace Lkt::Launch
