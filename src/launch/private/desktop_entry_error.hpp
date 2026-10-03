#pragma once

#include <cstdint>
#include <string_view>

namespace Lkt::Launch
{
enum class EDesktopEntryError : uint8_t
{
	NoDesktopEntryGroup,
	MalformedLine,
	UnknownEscape,
	InvalidBoolean
};

constexpr std::string_view ToString(EDesktopEntryError error)
{
	std::string_view text{ "unknown error" };

	switch (error)
	{
		case EDesktopEntryError::NoDesktopEntryGroup:
			text = "it has no [Desktop Entry] group";
			break;
		case EDesktopEntryError::MalformedLine:
			text = "a line in its [Desktop Entry] group is neither a key, a group nor a comment";
			break;
		case EDesktopEntryError::UnknownEscape:
			text = "a value holds an escape other than \\s \\n \\t \\r or \\\\";
			break;
		case EDesktopEntryError::InvalidBoolean:
			text = "a boolean is neither true, false, 1 nor 0";
			break;
	}

	return text;
}
} // namespace Lkt::Launch
