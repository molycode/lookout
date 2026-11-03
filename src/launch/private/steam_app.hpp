#pragma once

#include <filesystem>
#include <string>

namespace Lkt::Launch
{
struct SSteamApp final
{
	std::string name{};
	std::filesystem::path folder{};
};
} // namespace Lkt::Launch
