#include "game_order.hpp"
#include "json/json.hpp"
#include <algorithm>
#include <format>
#include <utility>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr bool AllowExceptions{ false };
constexpr bool IgnoreComments{ true };

//////////////////////////////////////////////////////////////////////////
void Fail(std::string& problem, std::string text)
{
	if (problem.empty())
	{
		problem = std::move(text);
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<std::vector<std::string>, std::string> ReadGameOrder(std::string_view text)
{
	JsonValue const root = JsonValue::parse(text, nullptr, AllowExceptions, IgnoreComments);
	std::vector<std::string> keys{};
	std::string problem{};

	if (!root.is_array())
	{
		problem = "it must hold an array of game keys";
	}
	else
	{
		for (JsonValue const& entry : root)
		{
			if (!entry.is_string() || entry.get_ref<std::string const&>().empty())
			{
				Fail(problem, "every entry must be a game key");
			}
			else if (std::ranges::contains(keys, entry.get_ref<std::string const&>()))
			{
				Fail(problem, std::format("'{}' is listed twice", entry.get_ref<std::string const&>()));
			}
			else
			{
				keys.emplace_back(entry.get<std::string>());
			}
		}
	}

	std::expected<std::vector<std::string>, std::string> result{ std::move(keys) };

	if (!problem.empty())
	{
		result = std::unexpected{ std::move(problem) };
	}

	return result;
}
} // namespace Lkt::Games
