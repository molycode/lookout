#include "json_layout.hpp"
#include <algorithm>
#include <cstddef>
#include <format>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr int Compact{ -1 };
constexpr char Space{ ' ' };
constexpr char IndentCharacter{ '\t' };
constexpr size_t MaxInlineLength{ 100 };

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
} // namespace

//////////////////////////////////////////////////////////////////////////
// As lookout-games writes them: a list or object of plain values on one line while it is short, the rest a line each.
std::string WriteJsonLayout(nlohmann::ordered_json const& value)
{
	std::string text{};

	Write(value, 0, text);

	return text;
}
} // namespace Lkt::Games
