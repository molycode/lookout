#include "merge_patch.hpp"
#include "json/json.hpp"
#include "json/syntax_error.hpp"
#include <algorithm>
#include <cstddef>
#include <format>
#include <utility>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr bool AllowExceptions{ false };
constexpr bool IgnoreComments{ false };
constexpr int Compact{ -1 };
constexpr char Space{ ' ' };
constexpr char IndentCharacter{ '\t' };
constexpr size_t MaxInlineLength{ 100 };

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
std::string DumpValue(JsonValue const& value)
{
	return value.dump(Compact, Space, false, JsonValue::error_handler_t::replace);
}

//////////////////////////////////////////////////////////////////////////
std::string DumpKey(std::string const& key)
{
	JsonValue const value = key;

	return std::format("{}: ", DumpValue(value));
}

//////////////////////////////////////////////////////////////////////////
// Empty for a container that holds another, or one too long for a line.
std::string WriteInline(JsonValue const& value)
{
	std::string text{};
	bool const isFlat{ std::ranges::none_of(value, [](JsonValue const& child) { return child.is_structured(); }) };

	if (isFlat && value.is_object())
	{
		for (auto const& item : value.items())
		{
			text += std::format("{}{}{}", text.empty() ? "{ " : ", ", DumpKey(item.key()), DumpValue(item.value()));
		}

		text += text.empty() ? "{}" : " }";
	}
	else if (isFlat)
	{
		for (JsonValue const& child : value)
		{
			text += std::format("{}{}", text.empty() ? "[ " : ", ", DumpValue(child));
		}

		text += text.empty() ? "[]" : " ]";
	}

	return (text.size() <= MaxInlineLength) ? text : std::string{};
}

//////////////////////////////////////////////////////////////////////////
void Write(JsonValue const& value, size_t depth, std::string& text)
{
	std::string const inlineText{ value.is_structured() ? WriteInline(value) : DumpValue(value) };

	if (!inlineText.empty())
	{
		text += inlineText;
	}
	else
	{
		std::string const indent(depth + 1, IndentCharacter);
		bool isFirst{ true };

		text += value.is_object() ? "{\n" : "[\n";

		for (auto const& item : value.items())
		{
			text += std::format("{}{}{}", isFirst ? "" : ",\n", indent, value.is_object() ? DumpKey(item.key()) : std::string{});
			Write(item.value(), depth + 1, text);
			isFirst = false;
		}

		text += std::format("\n{}{}", std::string(depth, IndentCharacter), value.is_object() ? "}" : "]");
	}
}

//////////////////////////////////////////////////////////////////////////
// As lookout-games writes them: a list or object of plain values on one line while it is short, the rest a line each.
std::string Dump(JsonValue const& value)
{
	std::string text{};

	Write(value, 0, text);

	return text;
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
		result = Dump(game);
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

		result = patch.empty() ? std::nullopt : std::optional<std::string>{ Dump(patch) };
	}
	else
	{
		result = std::unexpected{ game.error() };
	}

	return result;
}
} // namespace Lkt::Games
