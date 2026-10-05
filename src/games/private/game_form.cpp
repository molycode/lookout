#include "games/game_form.hpp"
#include "json_layout.hpp"
#include "games/game_fields.hpp"
#include "games/game_format.hpp"
#include "json/json.hpp"
#include "json/syntax_error.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <utility>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr bool AllowExceptions{ false };
constexpr bool IgnoreComments{ false };
constexpr std::string_view CommentPrefix{ "//" };
constexpr std::string_view ListSuffix{ "[]" };
constexpr std::string_view PaletteField{ "palette" };
constexpr std::string_view CodesField{ "codes" };
constexpr std::string_view RgbCodes{ "rgb" };
constexpr int HexBase{ 16 };
constexpr size_t HexDigits{ 4 };
constexpr unsigned char FirstPrintable{ 0x20 };
constexpr unsigned char Delete{ 0x7F };

//////////////////////////////////////////////////////////////////////////
std::string JoinPath(std::string_view parent, std::string_view key)
{
	return parent.empty() ? std::string{ key } : std::format("{}.{}", parent, key);
}

//////////////////////////////////////////////////////////////////////////
// Only the first problem is kept, so the message names where the file first went wrong.
void Fail(std::string& problem, std::string_view path, std::string_view reason)
{
	if (problem.empty())
	{
		problem = path.empty() ? std::string{ reason } : std::format("{}: {}", path, reason);
	}
}

//////////////////////////////////////////////////////////////////////////
SGameField const* FindField(std::string_view parent, std::string_view name)
{
	std::span<SGameField const> const fields{ GetGameFields() };
	auto const it{ std::ranges::find_if(fields, [parent, name](SGameField const& field) { return field.parent == parent && field.name == name; }) };

	return (it != fields.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
// The path the table names a field's own fields by: "text.colourCodes", or "masters[]" for each object in masters.
std::string ToSchemaPath(SGameField const& field)
{
	std::string path{ JoinPath(field.parent, field.name) };

	if (field.kind == EGameFieldKind::GroupList)
	{
		path += ListSuffix;
	}

	return path;
}

//////////////////////////////////////////////////////////////////////////
// A string as it is, anything else as its JSON, so a value of the wrong type is shown for the check to name.
std::string ToText(JsonValue const& value)
{
	return value.is_string() ? value.get<std::string>() : value.dump();
}

//////////////////////////////////////////////////////////////////////////
// Control characters, such as UT2004's ESC, cannot be typed, so they and the backslash that writes them are escaped.
std::string EscapeCharacters(std::string_view text)
{
	std::string escaped{};

	for (char const c : text)
	{
		unsigned char const byte{ static_cast<unsigned char>(c) };

		if (c == '\\')
		{
			escaped += "\\\\";
		}
		else if (byte < FirstPrintable || byte == Delete)
		{
			escaped += std::format("\\u{:04x}", byte);
		}
		else
		{
			escaped += c;
		}
	}

	return escaped;
}

//////////////////////////////////////////////////////////////////////////
void AppendUtf8(uint32_t codePoint, std::string& text)
{
	if (codePoint < 0x80)
	{
		text += static_cast<char>(codePoint);
	}
	else if (codePoint < 0x800)
	{
		text += static_cast<char>(0xC0 | (codePoint >> 6));
		text += static_cast<char>(0x80 | (codePoint & 0x3F));
	}
	else
	{
		text += static_cast<char>(0xE0 | (codePoint >> 12));
		text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
		text += static_cast<char>(0x80 | (codePoint & 0x3F));
	}
}

//////////////////////////////////////////////////////////////////////////
// A backslash that starts neither "\\" nor "\uXXXX" is kept as typed.
std::string UnescapeCharacters(std::string_view text)
{
	std::string unescaped{};
	size_t at{ 0 };

	while (at < text.size())
	{
		std::string_view const rest{ text.substr(at) };
		uint32_t codePoint{ 0 };
		bool const isUnicode{ rest.starts_with("\\u") && rest.size() >= 2 + HexDigits
			&& std::from_chars(rest.data() + 2, rest.data() + 2 + HexDigits, codePoint, HexBase).ptr == rest.data() + 2 + HexDigits };

		if (rest.starts_with("\\\\"))
		{
			unescaped += '\\';
			at += 2;
		}
		else if (isUnicode)
		{
			AppendUtf8(codePoint, unescaped);
			at += 2 + HexDigits;
		}
		else
		{
			unescaped += rest.front();
			++at;
		}
	}

	return unescaped;
}

//////////////////////////////////////////////////////////////////////////
SGameFormNode MakeAbsent(SGameField const& field);

//////////////////////////////////////////////////////////////////////////
// Every field the table lists under the parent and the node lacks, in the table's order.
void AppendAbsentFields(std::string_view schemaParent, SGameFormNode& node)
{
	for (SGameField const& field : GetGameFields())
	{
		if (field.parent == schemaParent && FindFormField(node, field.name) == nullptr)
		{
			node.children.emplace_back(MakeAbsent(field));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
SGameFormNode MakeAbsent(SGameField const& field)
{
	SGameFormNode node{};

	node.name = field.name;

	if (field.kind == EGameFieldKind::Group)
	{
		AppendAbsentFields(ToSchemaPath(field), node);
	}

	return node;
}

//////////////////////////////////////////////////////////////////////////
// A note must sit beside its field, as the reader asks, or the form would write it where it no longer belongs.
void ReadNotes(JsonValue const& object, std::string_view path, SGameFormNode& node, std::string& problem)
{
	for (auto const& item : object.items())
	{
		std::string_view const key{ item.key() };

		if (key.starts_with(CommentPrefix))
		{
			std::string_view const fieldName{ key.substr(CommentPrefix.size()) };
			auto const field{ std::ranges::find(node.children, fieldName, &SGameFormNode::name) };
			bool const isText{ item.value().is_string() && !item.value().get_ref<std::string const&>().empty()
				&& !item.value().get_ref<std::string const&>().contains('\0') };

			if (field == node.children.end())
			{
				Fail(problem, JoinPath(path, key), std::format("is a comment on '{}', which is not in this object", fieldName));
			}
			else if (!isText)
			{
				Fail(problem, JoinPath(path, key), "must be a non-empty string without NUL");
			}
			else
			{
				field->note = item.value().get<std::string>();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadFields(JsonValue const& object, std::string_view schemaParent, std::string_view path, SGameFormNode& node, std::string& problem);

//////////////////////////////////////////////////////////////////////////
void ReadOptions(JsonValue const& object, std::string_view path, SGameFormNode& node, std::string& problem)
{
	for (auto const& item : object.items())
	{
		if (!std::string_view{ item.key() }.starts_with(CommentPrefix))
		{
			SGameFormNode& option{ node.children.emplace_back() };

			option.name = item.key();
			option.text = ToText(item.value());
			option.isPresent = true;
		}
	}

	ReadNotes(object, path, node, problem);
}

//////////////////////////////////////////////////////////////////////////
SGameFormNode ReadField(JsonValue const& value, SGameField const& field, std::string_view path, std::string& problem)
{
	SGameFormNode node{};

	node.name = field.name;
	node.isPresent = true;

	switch (field.kind)
	{
		case EGameFieldKind::Version:
			if (value.is_number_unsigned() && value.get<uint64_t>() > GameFormat)
			{
				Fail(problem, path, std::format("is {}, which needs a newer Lookout", value.get<uint64_t>()));
			}

			node.text = ToText(value);
			break;
		case EGameFieldKind::Characters:
			node.text = value.is_string() ? EscapeCharacters(value.get_ref<std::string const&>()) : value.dump();
			break;
		case EGameFieldKind::Text:
		case EGameFieldKind::Number:
		case EGameFieldKind::Choice:
		case EGameFieldKind::Protocol:
			node.text = ToText(value);
			break;
		case EGameFieldKind::TextList:
			if (value.is_array())
			{
				for (JsonValue const& entry : value)
				{
					node.texts.emplace_back(ToText(entry));
				}
			}
			else
			{
				Fail(problem, path, "must be an array");
			}

			break;
		case EGameFieldKind::Group:
			if (value.is_object())
			{
				ReadFields(value, ToSchemaPath(field), path, node, problem);
			}
			else
			{
				Fail(problem, path, "must be an object");
			}

			break;
		case EGameFieldKind::GroupList:
			if (value.is_array())
			{
				size_t index{ 0 };

				for (JsonValue const& entry : value)
				{
					std::string const entryPath{ std::format("{}[{}]", path, index) };
					SGameFormNode& item{ node.children.emplace_back() };

					item.isPresent = true;

					if (entry.is_object())
					{
						ReadFields(entry, ToSchemaPath(field), entryPath, item, problem);
					}
					else
					{
						Fail(problem, entryPath, "must be an object");
					}

					++index;
				}
			}
			else
			{
				Fail(problem, path, "must be an array");
			}

			break;
		case EGameFieldKind::ProtocolOptions:
			if (value.is_object())
			{
				ReadOptions(value, path, node, problem);
			}
			else
			{
				Fail(problem, path, "must be an object");
			}

			break;
	}

	return node;
}

//////////////////////////////////////////////////////////////////////////
// In the file's order, so a description written back unchanged makes no patch; the fields it lacks follow.
void ReadFields(JsonValue const& object, std::string_view schemaParent, std::string_view path, SGameFormNode& node, std::string& problem)
{
	for (auto const& item : object.items())
	{
		std::string_view const key{ item.key() };
		SGameField const* const pField{ FindField(schemaParent, key) };

		if (pField != nullptr)
		{
			node.children.emplace_back(ReadField(item.value(), *pField, JoinPath(path, key), problem));
		}
		else if (!key.starts_with(CommentPrefix))
		{
			Fail(problem, JoinPath(path, key), "is not a field of format 1");
		}
	}

	ReadNotes(object, path, node, problem);
	AppendAbsentFields(schemaParent, node);
}

//////////////////////////////////////////////////////////////////////////
// A whole number becomes one; anything else stays text, for the check to name.
JsonValue WriteNumber(std::string_view text)
{
	int64_t number{ 0 };
	std::from_chars_result const result{ std::from_chars(text.data(), text.data() + text.size(), number) };
	bool const isNumber{ result.ec == std::errc{} && result.ptr == text.data() + text.size() };
	JsonValue value = std::string{ text };

	if (isNumber && number >= 0)
	{
		value = static_cast<uint64_t>(number);
	}
	else if (isNumber)
	{
		value = number;
	}

	return value;
}

//////////////////////////////////////////////////////////////////////////
bool HasText(SGameFormNode const& node)
{
	return !node.text.empty();
}

//////////////////////////////////////////////////////////////////////////
JsonValue WriteFields(SGameFormNode const& node, std::string_view schemaParent);

//////////////////////////////////////////////////////////////////////////
// Rgb codes carry their colour, so a palette kept from other codes is not written for them.
bool IsPaletteUnused(SGameField const& field, SGameFormNode const& siblings)
{
	SGameFormNode const* const pCodes{ FindFormField(siblings, CodesField) };

	return field.name == PaletteField && pCodes != nullptr && pCodes->text == RgbCodes;
}

//////////////////////////////////////////////////////////////////////////
std::optional<JsonValue> WriteField(SGameFormNode const& node, SGameField const& field, SGameFormNode const& siblings)
{
	std::optional<JsonValue> value{};

	switch (field.kind)
	{
		case EGameFieldKind::Version:
			value = static_cast<uint64_t>(GameFormat);
			break;
		case EGameFieldKind::Text:
		case EGameFieldKind::Choice:
		case EGameFieldKind::Protocol:
			if (HasText(node))
			{
				value = node.text;
			}

			break;
		case EGameFieldKind::Characters:
			if (HasText(node))
			{
				value = UnescapeCharacters(node.text);
			}

			break;
		case EGameFieldKind::Number:
			if (HasText(node))
			{
				value = WriteNumber(node.text);
			}

			break;
		case EGameFieldKind::TextList:
			if ((field.isRequired || node.isPresent || !node.texts.empty()) && !IsPaletteUnused(field, siblings))
			{
				JsonValue list = JsonValue::array();

				for (std::string const& text : node.texts)
				{
					list.emplace_back(text);
				}

				value = std::move(list);
			}

			break;
		case EGameFieldKind::Group:
			if (field.isRequired || node.isPresent)
			{
				value = WriteFields(node, ToSchemaPath(field));
			}

			break;
		case EGameFieldKind::GroupList:
			if (field.isRequired || node.isPresent || !node.children.empty())
			{
				JsonValue list = JsonValue::array();

				for (SGameFormNode const& item : node.children)
				{
					list.emplace_back(WriteFields(item, ToSchemaPath(field)));
				}

				value = std::move(list);
			}

			break;
		case EGameFieldKind::ProtocolOptions:
			if (node.isPresent || std::ranges::any_of(node.children, HasText))
			{
				JsonValue options = JsonValue::object();

				for (SGameFormNode const& option : node.children)
				{
					if (HasText(option))
					{
						if (!option.note.empty())
						{
							options[std::format("{}{}", CommentPrefix, option.name)] = option.note;
						}

						options[option.name] = option.text;
					}
				}

				value = std::move(options);
			}

			break;
	}

	return value;
}

//////////////////////////////////////////////////////////////////////////
// A note goes just before its field, as lookout-games writes them, and only with it.
JsonValue WriteFields(SGameFormNode const& node, std::string_view schemaParent)
{
	JsonValue object = JsonValue::object();

	for (SGameFormNode const& child : node.children)
	{
		SGameField const* const pField{ FindField(schemaParent, child.name) };
		std::optional<JsonValue> value{ (pField != nullptr) ? WriteField(child, *pField, node) : std::nullopt };

		if (value.has_value())
		{
			if (!child.note.empty())
			{
				object[std::format("{}{}", CommentPrefix, child.name)] = child.note;
			}

			object[child.name] = std::move(*value);
		}
	}

	return object;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<SGameFormNode, std::string> ReadGameForm(std::string_view text)
{
	JsonValue const root = JsonValue::parse(text, nullptr, AllowExceptions, IgnoreComments);
	SGameFormNode form{};
	std::string problem{};

	form.isPresent = true;

	if (root.is_discarded())
	{
		Fail(problem, {}, std::format("it is not valid JSON: {}", Json::DescribeSyntaxError(text, IgnoreComments)));
	}
	else if (!root.is_object())
	{
		Fail(problem, {}, "it must hold a JSON object");
	}
	else
	{
		ReadFields(root, {}, {}, form, problem);
	}

	std::expected<SGameFormNode, std::string> result{ std::move(form) };

	if (!problem.empty())
	{
		result = std::unexpected{ std::move(problem) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::string WriteGameForm(SGameFormNode const& form)
{
	return WriteJsonLayout(WriteFields(form, {}));
}

//////////////////////////////////////////////////////////////////////////
SGameFormNode MakeGameForm()
{
	SGameFormNode form{};

	form.isPresent = true;
	AppendAbsentFields({}, form);

	return form;
}

//////////////////////////////////////////////////////////////////////////
SGameFormNode MakeGameFormItem(std::string_view listPath)
{
	SGameFormNode item{};

	item.isPresent = true;
	AppendAbsentFields(std::format("{}{}", listPath, ListSuffix), item);

	return item;
}

//////////////////////////////////////////////////////////////////////////
SGameFormNode const* FindFormField(SGameFormNode const& node, std::string_view name)
{
	auto const it{ std::ranges::find(node.children, name, &SGameFormNode::name) };

	return (it != node.children.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
SGameFormNode& GetFormField(SGameFormNode& node, std::string_view name)
{
	auto it{ std::ranges::find(node.children, name, &SGameFormNode::name) };

	if (it == node.children.end())
	{
		node.children.emplace_back().name = name;
		it = std::prev(node.children.end());
	}

	return *it;
}

//////////////////////////////////////////////////////////////////////////
void ClearFormNotes(SGameFormNode& node)
{
	node.note.clear();

	for (SGameFormNode& child : node.children)
	{
		ClearFormNotes(child);
	}
}
} // namespace Lkt::Games
