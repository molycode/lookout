#pragma once

#include "net/master_failed.hpp"
#include "net/refresh_finished.hpp"
#include "net/server_answered.hpp"
#include "net/server_failed.hpp"
#include "net/servers_listed.hpp"
#include <variant>

namespace Lkt::Net
{
// Each event carries its refresh's id: one older than the game's latest Refresh is left over from a replaced refresh.
using SQueryEvent = std::variant<SServersListed, SMasterFailed, SServerAnswered, SServerFailed, SRefreshFinished>;
} // namespace Lkt::Net
