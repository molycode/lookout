#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace Lkt::Launch
{
struct SGogInstall final
{
	uint64_t id{ 0 };
	std::filesystem::path folder{};
	std::string name{};
};
} // namespace Lkt::Launch
