#include "install_folder.hpp"
#include "loggers.hpp"
#include "program_path.hpp"
#include <algorithm>
#include <format>
#include <system_error>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
std::expected<std::filesystem::path, std::string> CheckInstallFolder(std::filesystem::path const& folder, std::string_view program,
	std::span<std::string const> requiredFiles)
{
	std::expected<std::filesystem::path, std::string> result{ std::unexpected{ std::string{ "it is not a folder" } } };
	std::error_code error{};

	if (std::filesystem::is_directory(folder, error))
	{
		std::filesystem::path const path{ folder / program };
		std::expected<void, EProgramError> const check{ CheckExecutable(path) };
		auto const missing{ std::ranges::find_if(requiredFiles, [&folder](std::string_view file)
		{
			std::error_code existsError{};

			return !std::filesystem::exists(folder / file, existsError);
		}) };

		if (missing != requiredFiles.end())
		{
			result = std::unexpected{ std::format("'{}' is missing", *missing) };
		}
		else if (!check.has_value())
		{
			result = std::unexpected{ std::format("'{}' {}", program, ToString(check.error())) };
		}
		else
		{
			result = path;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// A folder found where the game should be but unable to start it is worth a warning; the game may be broken.
std::optional<std::filesystem::path> FindInstallProgram(std::filesystem::path const& folder, std::string_view program,
	std::span<std::string const> requiredFiles, std::string_view gameName)
{
	std::expected<std::filesystem::path, std::string> const checked{ CheckInstallFolder(folder, program, requiredFiles) };
	std::optional<std::filesystem::path> found{};

	if (checked.has_value())
	{
		found = *checked;
		found->make_preferred();
	}
	else
	{
		gLog.Warning("'{}' is not used to start {}: {}", folder.string(), gameName, checked.error());
	}

	return found;
}
} // namespace Lkt::Launch
