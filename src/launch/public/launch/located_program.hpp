#pragma once

#include <filesystem>
#include <string>

namespace Lkt::Launch
{
struct SLocatedProgram final
{
	std::string key{};
	std::filesystem::path program{};
};
} // namespace Lkt::Launch
