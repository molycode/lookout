#pragma once

#include <span>
#include <string_view>

namespace Lkt::Embedded
{
inline std::string_view AsText(std::span<unsigned char const> bytes)
{
	return std::string_view{ reinterpret_cast<char const*>(bytes.data()), bytes.size() };
}
} // namespace Lkt::Embedded
