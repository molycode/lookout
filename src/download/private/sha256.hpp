#pragma once

#include <string>
#include <string_view>

namespace Lkt::Download
{
// Lower-case hex, as index.json writes it.
std::string HashSha256(std::string_view bytes);
} // namespace Lkt::Download
