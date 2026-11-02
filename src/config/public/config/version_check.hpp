#pragma once

#include <string_view>

namespace Lkt::Config
{
bool IsVersionCheckOn(std::string_view configDir);
bool SetVersionCheck(std::string_view configDir, bool isOn);
} // namespace Lkt::Config
