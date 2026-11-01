#pragma once

#include <cstdint>
#include <string_view>

namespace Lkt::Launch
{
enum class EKeyValuesError : uint8_t
{
	UnterminatedString,
	UnclosedGroup,
	UnexpectedClose,
	MissingValue,
	UnexpectedCharacter,
	TooDeep
};

constexpr std::string_view ToString(EKeyValuesError error)
{
	std::string_view text{ "unknown error" };

	switch (error)
	{
		case EKeyValuesError::UnterminatedString:
			text = "a quoted string is not closed";
			break;
		case EKeyValuesError::UnclosedGroup:
			text = "a group is not closed";
			break;
		case EKeyValuesError::UnexpectedClose:
			text = "a group is closed that was never opened";
			break;
		case EKeyValuesError::MissingValue:
			text = "a key has neither a value nor a group";
			break;
		case EKeyValuesError::UnexpectedCharacter:
			text = "it holds something other than quoted strings, braces and comments";
			break;
		case EKeyValuesError::TooDeep:
			text = "its groups are nested too deeply";
			break;
	}

	return text;
}
} // namespace Lkt::Launch
