#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Games
{
// order.json: the built-in game keys in sidebar order; the error says what is wrong with it.
std::expected<std::vector<std::string>, std::string> ReadGameOrder(std::string_view text);
} // namespace Lkt::Games
