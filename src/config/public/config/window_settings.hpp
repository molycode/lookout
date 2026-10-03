#pragma once

#include <cstdint>
#include <string>

namespace Lkt::Config
{
// Logical sizes, divided by the UI scale, so one default suits every display.
struct SWindowSettings final
{
	uint32_t width{ 1280 };
	uint32_t height{ 800 };
	bool isMaximized{ false };
	uint32_t detailsWidth{ 288 };
	std::string layout;

	bool operator==(SWindowSettings const&) const = default;
};
} // namespace Lkt::Config
