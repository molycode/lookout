#pragma once

#include "net/loopback_stream.hpp"
#include "query/server_address.hpp"
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Fixtures
{
inline constexpr std::string_view Gamespy1Challenge{ "\\basic\\\\secure\\WVNJYU" };

std::string MakeGamespy1ListRequest(std::string_view masterGame);
SLoopbackStream MakeGamespy1Master(std::string_view masterGame, std::span<Query::SServerAddress const> servers);
} // namespace Lkt::Fixtures
