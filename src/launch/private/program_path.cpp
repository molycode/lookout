#include "program_path.hpp"
#include "path_list.hpp"
#include <tge/process/is_runnable.hpp>
#include <system_error>
#include <vector>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
std::expected<void, EProgramError> CheckExecutable(std::filesystem::path const& path)
{
	std::expected<void, EProgramError> result{};
	std::error_code error{};
	std::filesystem::file_status const status{ std::filesystem::status(path, error) };

	if (!std::filesystem::exists(status))
	{
		result = std::unexpected{ EProgramError::NotFound };
	}
	else if (!std::filesystem::is_regular_file(status))
	{
		result = std::unexpected{ EProgramError::NotRegularFile };
	}
	else if (!Tge::Process::IsRunnable(path))
	{
		result = std::unexpected{ EProgramError::NotExecutable };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<std::filesystem::path, EProgramError> ResolveProgram(std::string_view program, std::filesystem::path const& workingDir, std::string_view searchPath)
{
	std::expected<std::filesystem::path, EProgramError> result{ std::unexpected{ EProgramError::NotFound } };

	if (program.contains('/'))
	{
		std::filesystem::path path{ program };

		if (path.is_relative())
		{
			std::error_code error{};

			path = workingDir.empty() ? std::filesystem::absolute(path, error) : workingDir / path;
		}

		// Normalised before the check, so the path checked is the one that runs.
		path = path.lexically_normal();

		std::expected<void, EProgramError> const check{ CheckExecutable(path) };

		if (check.has_value())
		{
			result = path;
		}
		else
		{
			result = std::unexpected{ check.error() };
		}
	}
	else if (!program.empty())
	{
		std::vector<std::filesystem::path> const directories{ SplitPathList(searchPath) };

		for (size_t index{ 0 }; !result.has_value() && index < directories.size(); ++index)
		{
			std::filesystem::path const candidate{ directories[index] / program };

			if (CheckExecutable(candidate).has_value())
			{
				result = candidate;
			}
		}
	}

	return result;
}
} // namespace Lkt::Launch
