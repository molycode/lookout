#pragma once

#include <cstdint>
#include <string_view>

namespace Lkt::Launch
{
enum class ECommandLineError : uint8_t
{
	UnterminatedQuote,
	StrayBackslash,
	UnquotedShellCharacter,
	UnknownFieldCode
};

constexpr std::string_view ToString(ECommandLineError error)
{
	std::string_view text{ "unknown error" };

	switch (error)
	{
		case ECommandLineError::UnterminatedQuote:
			text = "a quote is never closed";
			break;
		case ECommandLineError::StrayBackslash:
			text = "a backslash escapes something other than \" ` $ or \\ inside quotes";
			break;
		case ECommandLineError::UnquotedShellCharacter:
			text = "a shell character is not in double quotes";
			break;
		case ECommandLineError::UnknownFieldCode:
			text = "it holds an unknown % field code";
			break;
	}

	return text;
}
} // namespace Lkt::Launch
