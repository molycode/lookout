#include "merge_patch.hpp"
#include "json_layout.hpp"
#include "json/json.hpp"
#include "json/syntax_error.hpp"
#include <format>
#include <utility>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr bool AllowExceptions{ false };
constexpr bool IgnoreComments{ false };

//////////////////////////////////////////////////////////////////////////
std::expected<JsonValue, std::string> ParseObject(std::string_view text, std::string_view notAnObject)
{
	JsonValue value = JsonValue::parse(text, nullptr, AllowExceptions, IgnoreComments);
	std::expected<JsonValue, std::string> result{ std::unexpected{ std::string{ notAnObject } } };

	if (value.is_object())
	{
		result = std::move(value);
	}
	else if (value.is_discarded())
	{
		result = std::unexpected{ std::format("it is not valid JSON: {}", Json::DescribeSyntaxError(text, IgnoreComments)) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Objects are compared field by field, so a change deep inside one names only that field; anything else is replaced whole.
JsonValue Diff(JsonValue const& from, JsonValue const& to)
{
	JsonValue patch = JsonValue::object();

	for (auto const& item : to.items())
	{
		JsonValue::const_iterator const old{ from.find(item.key()) };

		if (old != from.cend() && old->is_object() && item.value().is_object())
		{
			JsonValue changes = Diff(*old, item.value());

			if (!changes.empty())
			{
				patch[item.key()] = std::move(changes);
			}
		}
		else if (old == from.cend() || *old != item.value())
		{
			patch[item.key()] = item.value();
		}
	}

	for (auto const& item : from.items())
	{
		if (to.find(item.key()) == to.cend())
		{
			patch[item.key()] = nullptr;
		}
	}

	return patch;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<std::string, std::string> ApplyPatch(std::string_view baseText, std::string_view patchText)
{
	JsonValue game = JsonValue::parse(baseText, nullptr, AllowExceptions, IgnoreComments);
	std::expected<JsonValue, std::string> const patch{ ParseObject(patchText, "a change to a downloaded game must be a JSON object") };
	std::expected<std::string, std::string> result{};

	if (patch.has_value())
	{
		game.merge_patch(*patch);
		result = WriteJsonLayout(game);
	}
	else
	{
		result = std::unexpected{ patch.error() };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<std::optional<std::string>, std::string> MakePatch(std::string_view baseText, std::string_view gameText)
{
	JsonValue const base = JsonValue::parse(baseText, nullptr, AllowExceptions, IgnoreComments);
	std::expected<JsonValue, std::string> const game{ ParseObject(gameText, "it must hold a JSON object") };
	std::expected<std::optional<std::string>, std::string> result{};

	if (game.has_value())
	{
		JsonValue const patch = Diff(base, *game);

		result = patch.empty() ? std::nullopt : std::optional<std::string>{ WriteJsonLayout(patch) };
	}
	else
	{
		result = std::unexpected{ game.error() };
	}

	return result;
}
} // namespace Lkt::Games
