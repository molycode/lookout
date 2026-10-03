#pragma once

#include "config/settings.hpp"
#include <cstdint>
#include <string>

namespace Lkt::Config
{
struct SSettingsDocument final
{
	SSettings settings;
	uint32_t version{ 0 };
	uint32_t numInvalid{ 0 };
	std::string firstInvalidPath;
};
} // namespace Lkt::Config
