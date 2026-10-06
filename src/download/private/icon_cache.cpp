#include "icon_cache.hpp"
#include "index_file.hpp"
#include "sha256.hpp"
#include "json/files.hpp"
#include <format>
#include <system_error>
#include <utility>

namespace Lkt::Download
{
namespace
{
constexpr std::string_view IconExtension{ ".png" };

//////////////////////////////////////////////////////////////////////////
std::filesystem::path MakeIconPath(std::filesystem::path const& iconDir, std::string_view sha256)
{
	return iconDir / std::format("{}{}", sha256, IconExtension);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// Nothing when the icon is not cached.
std::expected<std::optional<std::string>, std::string> ReadCachedIcon(std::filesystem::path const& iconDir, SIndexFile const& icon)
{
	std::filesystem::path const path{ MakeIconPath(iconDir, icon.sha256) };
	std::expected<std::string, std::error_code> read{ Json::ReadFile(path, icon.size) };
	std::expected<std::optional<std::string>, std::string> result{ std::optional<std::string>{} };

	if (read.has_value() && read->size() == icon.size && HashSha256(*read) == icon.sha256)
	{
		result = std::optional<std::string>{ std::move(*read) };
	}
	else if (read.has_value() || read.error() != std::errc::no_such_file_or_directory)
	{
		result = std::unexpected{ std::format("{}: {}", path.string(), read.has_value() ? std::string{ "it differs from the icon it is named after" }
			: read.error().message()) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> CacheIcon(std::filesystem::path const& iconDir, std::string_view sha256, std::string_view png)
{
	std::filesystem::path const path{ MakeIconPath(iconDir, sha256) };
	std::error_code error{};

	std::filesystem::create_directories(iconDir, error);

	std::expected<void, std::string> const written{ (error.value() == 0) ? Json::WriteFileAtomically(path, png)
		: std::expected<void, std::string>{ std::unexpected{ error.message() } } };

	return written.has_value() ? written : std::unexpected{ std::format("{}: {}", path.string(), written.error()) };
}
} // namespace Lkt::Download
