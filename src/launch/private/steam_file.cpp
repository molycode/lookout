#include "steam_file.hpp"
#include <tge/io/read_whole_file.hpp>
#include <cstddef>
#include <system_error>
#include <utility>

namespace Lkt::Launch
{
namespace
{
// A library list of thousands of games is under 100 KiB, and an app manifest a few.
constexpr size_t MaxSteamFileSize{ 1024 * 1024 };
} // namespace

//////////////////////////////////////////////////////////////////////////
// The reason is a phrase for "Cannot read '<file>': <reason>".
std::expected<std::vector<SKeyValue>, std::string> ReadSteamFile(std::filesystem::path const& path)
{
	std::expected<std::string, std::error_code> const text{ Tge::IO::ReadWholeFile(path, MaxSteamFileSize) };
	std::expected<std::vector<SKeyValue>, std::string> result{ std::unexpected{ std::string{} } };

	if (text.has_value())
	{
		std::expected<std::vector<SKeyValue>, EKeyValuesError> parsed{ ParseKeyValues(*text) };

		result = parsed.has_value() ? std::expected<std::vector<SKeyValue>, std::string>{ std::move(*parsed) }
			: std::unexpected{ std::string{ ToString(parsed.error()) } };
	}
	else
	{
		result = std::unexpected{ text.error().message() };
	}

	return result;
}
} // namespace Lkt::Launch
