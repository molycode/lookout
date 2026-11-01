#include "steam_libraries.hpp"
#include "loggers.hpp"
#include "steam_file.hpp"
#include <algorithm>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>

namespace Lkt::Launch
{
namespace
{
constexpr std::string_view ListPrefix{ "libraryfolders/" };
constexpr std::string_view PathKey{ "/path" };

//////////////////////////////////////////////////////////////////////////
// A numbered library's folder: "libraryfolders/1" in Steam's old list, "libraryfolders/1/path" in its new one.
bool IsLibraryFolder(std::string_view path)
{
	std::string_view const rest{ path.starts_with(ListPrefix) ? path.substr(ListPrefix.size()) : std::string_view{} };
	std::string_view const number{ rest.substr(0, rest.find('/')) };
	std::string_view const tail{ rest.substr(number.size()) };

	return !number.empty() && std::ranges::all_of(number, [](char c) { return c >= '0' && c <= '9'; }) && (tail.empty() || tail == PathKey);
}

//////////////////////////////////////////////////////////////////////////
bool IsListed(std::vector<std::filesystem::path> const& libraries, std::filesystem::path const& folder)
{
	return std::ranges::any_of(libraries, [&folder](std::filesystem::path const& library)
	{
		std::error_code error{};

		return std::filesystem::equivalent(library, folder, error) && error.value() == 0;
	});
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// The listed libraries first, so they are shown as Steam writes them, then Steam's own folder, which the old list leaves
// out.
std::vector<std::filesystem::path> ReadSteamLibraries(std::filesystem::path const& steamFolder)
{
	std::vector<std::filesystem::path> libraries{};

	if (!steamFolder.empty())
	{
		std::filesystem::path const list{ steamFolder / "steamapps" / "libraryfolders.vdf" };
		std::error_code error{};

		if (std::filesystem::exists(list, error))
		{
			std::expected<std::vector<SKeyValue>, std::string> const read{ ReadSteamFile(list) };

			if (read.has_value())
			{
				for (SKeyValue const& entry : *read)
				{
					if (IsLibraryFolder(entry.path))
					{
						libraries.emplace_back(entry.value);
					}
				}
			}
			else
			{
				gLog.Warning("Cannot read Steam's library list '{}': {}", list.string(), read.error());
			}
		}
		else if (error.value() != 0 && error != std::errc::no_such_file_or_directory)
		{
			gLog.Warning("Cannot look for Steam's library list '{}': {}", list.string(), error.message());
		}

		if (!IsListed(libraries, steamFolder))
		{
			libraries.emplace_back(steamFolder);
		}
	}

	return libraries;
}
} // namespace Lkt::Launch
