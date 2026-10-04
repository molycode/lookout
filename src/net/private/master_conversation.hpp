#pragma once

#include "conversation_record.hpp"
#include "udp_socket.hpp"
#include "query/server_address.hpp"
#include <tge/threading/watch_id.hpp>
#include <optional>

namespace Lkt::Net
{
// A master talks on its own connected socket, so its datagrams can never be taken for another's.
struct SMasterConversation final
{
	SConversationRecord record;
	Query::SServerAddress address;
	CUdpSocket socket;
	std::optional<Tge::Threading::SWatchId> watch;
};
} // namespace Lkt::Net
