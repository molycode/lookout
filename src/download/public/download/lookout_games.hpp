#pragma once

#include "download/download_source.hpp"
#include <string_view>

namespace Lkt::Download
{
// molycode/lookout-games, as raw.githubusercontent.com serves it.
SDownloadSource GetLookoutGamesSource(std::string_view userAgent);
} // namespace Lkt::Download
