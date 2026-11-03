#pragma once

#include "query/store_libraries.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace Lkt::Launch
{
struct SFoundStore final
{
	std::string key{};
	std::string name{};
	std::vector<std::string> arguments{};
	Query::EStoreLibraries libraries{ Query::EStoreLibraries::Steam };
	std::filesystem::path program{};
	std::vector<std::filesystem::path> libraryFolders{};
	std::filesystem::path folder{};
};
} // namespace Lkt::Launch
