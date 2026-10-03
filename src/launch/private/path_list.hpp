#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
// Empty and relative entries are dropped: they would resolve against whatever the working directory happens to be.
std::vector<std::filesystem::path> SplitPathList(std::string_view list);
} // namespace Lkt::Launch
