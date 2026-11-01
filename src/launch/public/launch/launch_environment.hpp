#pragma once

#include "launch/gog_install.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace Lkt::Launch
{
struct SLaunchEnvironment final
{
	std::filesystem::path dataHome{};
	std::vector<std::filesystem::path> dataDirs{};
	std::filesystem::path home{};
	std::string searchPath{};
	std::filesystem::path programFiles{};
	std::filesystem::path programFilesX86{};
	std::filesystem::path localAppData{};
	std::filesystem::path systemDrive{};
	std::vector<std::filesystem::path> steamLibraries{};
	std::vector<SGogInstall> gogInstalls{};
};

SLaunchEnvironment ReadLaunchEnvironment();
} // namespace Lkt::Launch
