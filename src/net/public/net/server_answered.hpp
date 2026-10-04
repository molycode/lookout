#pragma once

#include "query/game.hpp"
#include "query/server_address.hpp"
#include "query/status_reply.hpp"
#include <cstdint>

namespace Lkt::Net
{
struct SServerAnswered final
{
	Query::EGame game{ Query::NoGame };
	Query::SServerAddress address;
	uint32_t pingMs{ 0 };
	Query::SStatusReply reply;
	uint32_t refreshId{ 0 };
};
} // namespace Lkt::Net
