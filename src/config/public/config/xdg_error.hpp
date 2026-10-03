#pragma once

#include <cstdint>
#include <string_view>

namespace Lkt::Config
{
enum class EXdgError : uint8_t
{
	NoHome
};

constexpr std::string_view ToString(EXdgError error)
{
	std::string_view text{ "unknown error" };

	switch (error)
	{
		case EXdgError::NoHome:
			text = "HOME is unset or not an absolute path";
			break;
	}

	return text;
}
} // namespace Lkt::Config
