#include "key_values.hpp"
#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

namespace Lkt::Launch
{
namespace
{
// Steam's own files nest a few levels; a deeper one is broken, and would make every value copy a long path.
constexpr size_t MaxDepth{ 32 };
constexpr std::string_view CommentStart{ "//" };

//////////////////////////////////////////////////////////////////////////
void SkipBlanks(std::string_view text, size_t& at)
{
	bool isSkipping{ true };

	while (isSkipping && at < text.size())
	{
		char const c{ text[at] };

		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
		{
			++at;
		}
		else if (text.substr(at).starts_with(CommentStart))
		{
			at = std::min(text.find('\n', at), text.size());
		}
		else
		{
			isSkipping = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Only \\ and \" are decoded: Steam writes paths with doubled backslashes, and some tools with single ones.
std::expected<std::string, EKeyValuesError> ReadQuoted(std::string_view text, size_t& at)
{
	std::string value{};
	bool isClosed{ false };

	++at;

	while (!isClosed && at < text.size())
	{
		char const c{ text[at] };
		bool const isEscape{ c == '\\' && at + 1 < text.size() && (text[at + 1] == '\\' || text[at + 1] == '"') };

		if (c == '"')
		{
			isClosed = true;
			++at;
		}
		else if (isEscape)
		{
			value += text[at + 1];
			at += 2;
		}
		else
		{
			value += c;
			++at;
		}
	}

	return isClosed ? std::expected<std::string, EKeyValuesError>{ std::move(value) } : std::unexpected{ EKeyValuesError::UnterminatedString };
}

//////////////////////////////////////////////////////////////////////////
// Steam reads keys whatever their case, so they are kept lower-cased.
std::string ToLower(std::string_view text)
{
	std::string lower{ text };

	for (char& c : lower)
	{
		c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
	}

	return lower;
}

//////////////////////////////////////////////////////////////////////////
std::string JoinPath(std::vector<std::string> const& groups, std::string_view key)
{
	std::string path{};

	for (std::string const& group : groups)
	{
		path += group;
		path += '/';
	}

	return path + std::string{ key };
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// Each value under the keys that lead to it, joined by '/', lower-cased, such as "libraryfolders/0/path".
std::expected<std::vector<SKeyValue>, EKeyValuesError> ParseKeyValues(std::string_view text)
{
	std::vector<SKeyValue> values{};
	std::vector<std::string> groups{};
	std::optional<EKeyValuesError> error{};
	size_t at{ 0 };

	SkipBlanks(text, at);

	while (!error.has_value() && at < text.size())
	{
		if (text[at] == '}' && groups.empty())
		{
			error = EKeyValuesError::UnexpectedClose;
		}
		else if (text[at] == '}')
		{
			groups.pop_back();
			++at;
		}
		else if (text[at] == '"')
		{
			std::expected<std::string, EKeyValuesError> const key{ ReadQuoted(text, at) };

			SkipBlanks(text, at);

			if (!key.has_value())
			{
				error = key.error();
			}
			else if (at < text.size() && text[at] == '"')
			{
				std::expected<std::string, EKeyValuesError> value{ ReadQuoted(text, at) };

				if (value.has_value())
				{
					values.emplace_back(JoinPath(groups, ToLower(*key)), std::move(*value));
				}
				else
				{
					error = value.error();
				}
			}
			else if (at < text.size() && text[at] == '{' && groups.size() < MaxDepth)
			{
				groups.emplace_back(ToLower(*key));
				++at;
			}
			else
			{
				error = (at < text.size() && text[at] == '{') ? EKeyValuesError::TooDeep : EKeyValuesError::MissingValue;
			}
		}
		else
		{
			error = EKeyValuesError::UnexpectedCharacter;
		}

		SkipBlanks(text, at);
	}

	if (!error.has_value() && !groups.empty())
	{
		error = EKeyValuesError::UnclosedGroup;
	}

	std::expected<std::vector<SKeyValue>, EKeyValuesError> result{ std::move(values) };

	if (error.has_value())
	{
		result = std::unexpected{ *error };
	}

	return result;
}
} // namespace Lkt::Launch
