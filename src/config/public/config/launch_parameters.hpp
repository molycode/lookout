#pragma once

#include <string>

namespace Lkt::Config
{
struct SLaunchParameters final
{
	std::string launcherId{};
	std::string text{};

	bool operator==(SLaunchParameters const&) const = default;
};
} // namespace Lkt::Config
