#pragma once

#include "launch/gog_install.hpp"
#include "registry_root.hpp"
#include <filesystem>
#include <string_view>
#include <vector>

namespace Lkt::Query
{
struct SStoreRegistryValue;
} // namespace Lkt::Query

namespace Lkt::Launch
{
std::filesystem::path ReadRegistryFolder(ERegistryRoot root, std::string_view key, std::string_view value);
std::filesystem::path ReadStoreFolder(Query::SStoreRegistryValue const& folder);
std::vector<SGogInstall> ReadGogInstalls(ERegistryRoot root, std::string_view key);
} // namespace Lkt::Launch
