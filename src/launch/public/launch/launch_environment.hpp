#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Lkt::Launch
{
struct SLaunchEnvironment final
{
	std::filesystem::path dataHome;
	std::vector<std::filesystem::path> dataDirs;
	std::filesystem::path home;
	std::string searchPath;
};

SLaunchEnvironment ReadLaunchEnvironment();
} // namespace Lkt::Launch
