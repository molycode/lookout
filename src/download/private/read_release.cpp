#include "read_release.hpp"
#include "download/lookout_release.hpp"
#include "json/json.hpp"
#include "json/syntax_error.hpp"
#include <format>

namespace Lkt::Download
{
namespace
{
using JsonValue = nlohmann::json;

constexpr bool AllowExceptions{ false };
constexpr bool IgnoreComments{ false };
} // namespace

//////////////////////////////////////////////////////////////////////////
// Lookout's releases are tagged "v1.4.0".
std::expected<std::string, std::string> ReadReleaseVersion(std::string_view text)
{
	JsonValue const root = JsonValue::parse(text, nullptr, AllowExceptions, IgnoreComments);
	JsonValue::const_iterator const tag{ root.is_object() ? root.find("tag_name") : root.cend() };
	bool const isTagged{ tag != root.cend() && tag->is_string() };
	std::string_view const tagName{ isTagged ? std::string_view{ tag->get_ref<std::string const&>() } : std::string_view{} };
	std::string_view const number{ tagName.starts_with('v') ? tagName.substr(1) : tagName };
	std::expected<std::string, std::string> version{ std::string{ number } };

	if (root.is_discarded())
	{
		version = std::unexpected{ std::format("the latest release is not valid JSON: {}", Json::DescribeSyntaxError(text, IgnoreComments)) };
	}
	else if (!isTagged)
	{
		version = std::unexpected{ std::string{ "the latest release names no tag" } };
	}
	else if (!ParseLookoutVersion(number).has_value())
	{
		version = std::unexpected{ std::format("the latest release's tag, {}, is not a version such as v1.4.0", tagName) };
	}

	return version;
}
} // namespace Lkt::Download
