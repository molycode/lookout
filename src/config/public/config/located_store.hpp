#pragma once

#include <string>

namespace Lkt::Config
{
struct SLocatedStore final
{
	std::string key{};
	std::string program{};

	bool operator==(SLocatedStore const&) const = default;
};
} // namespace Lkt::Config
