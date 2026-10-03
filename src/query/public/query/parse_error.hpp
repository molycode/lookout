#pragma once

#include <cstdint>
#include <string_view>

namespace Lkt::Query
{
enum class EParseError : uint8_t
{
	WrongHeader,
	Truncated,
	Malformed
};

constexpr std::string_view ToString(EParseError error)
{
	std::string_view text{ "unknown error" };

	switch (error)
	{
		case EParseError::WrongHeader:
			text = "wrong header";
			break;

		case EParseError::Truncated:
			text = "truncated";
			break;

		case EParseError::Malformed:
			text = "malformed";
			break;
	}

	return text;
}
} // namespace Lkt::Query
