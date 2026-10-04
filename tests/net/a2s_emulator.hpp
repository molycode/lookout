#pragma once

#include "net/a2s_server_setup.hpp"
#include "net/loopback_exchange.hpp"
#include "query/server_address.hpp"
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace Lkt::Fixtures
{
// What the emulated servers say; the tests read it back from the replies.
inline constexpr std::string_view A2sServerName{ "Emulated Server" };
inline constexpr std::string_view A2sRule{ "rule59" };
inline constexpr std::string_view A2sRuleValue{ "value59" };
inline constexpr std::string_view A2sFilter{ "\\appid\\440" };

// The exchanges of a Source server: every query challenged when it asks for that, rules split over three datagrams
// that leave out of order, and each extra data field of A2S_INFO set.
std::vector<SLoopbackExchange> MakeA2sServer(SA2sServerSetup const& setup);
inline constexpr size_t A2sChallengedInfoExchange{ 1 };
inline constexpr size_t A2sUnchallengedPlayersExchange{ 2 };

// A Steam master listing its servers on two pages; the first request for the second page goes unanswered.
std::vector<SLoopbackExchange> MakeSteamMaster(Query::SServerAddress const& firstPage, Query::SServerAddress const& secondPage);
inline constexpr size_t SteamSecondPageExchange{ 1 };
} // namespace Lkt::Fixtures
