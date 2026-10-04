#pragma once

#include "conversation_record.hpp"
#include "stream_socket.hpp"
#include "udp_socket.hpp"
#include "query/server_address.hpp"
#include <tge/threading/watch_id.hpp>
#include <cstddef>
#include <optional>

namespace Lkt::Net
{
// A master talks on a socket of its own, a connected datagram socket or a stream, so what it sends can never be taken
// for another's. Only the socket its transport needs is opened.
struct SMasterConversation final
{
	SConversationRecord record;
	Query::SServerAddress address;
	CUdpSocket datagramSocket;
	CStreamSocket streamSocket;
	std::optional<Tge::Threading::SWatchId> watch;
	size_t numStreamBytes{ 0 };
	bool isStream{ false };
};
} // namespace Lkt::Net
