#pragma once

#include "net/loopback_exchange.hpp"
#include "net/loopback_stream.hpp"
#include "net/ut2004_server_setup.hpp"
#include "query/server_address.hpp"
#include <chrono>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace Lkt::Fixtures
{
// What the emulated servers say, as UTF-8; the tests read it back from the replies.
inline constexpr std::string_view Ut2004Map{ "DM-Rankin" };
inline constexpr std::string_view Ut2004PlainName{ "Red Blu\xC3\xA9" };
inline constexpr std::string_view Ut2004AdminName{ "\xC3\x9Cn\xC3\xAF" "c\xC3\xB8" "d\xC3\xA9 \xE2\x9C\x93 \xF0\x9F\x98\x80" };
inline constexpr std::string_view Ut2004WidePlayer{ "B\xC3\xB6" "b\xE2\x9C\x93" };
inline constexpr size_t Ut2004MotdLength{ 100 };

// A UT2004 server on its query port, each command answered in packets the set interval apart: the rules in two (a UTF-16
// string, a string whose length takes two bytes), the players in two.
std::vector<SLoopbackExchange> MakeUt2004Server(SUt2004ServerSetup const& setup);
inline constexpr size_t Ut2004InfoExchange{ 0 };
inline constexpr size_t Ut2004RulesExchange{ 1 };
inline constexpr size_t Ut2004PlayersExchange{ 2 };

// A UT2004 master: challenge, approval, verification, then a frame per server in odd-sized writes, pausing after the
// first, then it closes. Each listed server is a query address, and its game port the one below.
SLoopbackStream MakeUt2004Master(std::span<Query::SServerAddress const> servers, std::chrono::milliseconds listPause = std::chrono::milliseconds{ 0 });
} // namespace Lkt::Fixtures
