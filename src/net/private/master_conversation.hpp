#pragma once

#include "conversation_record.hpp"
#include "query/server_address.hpp"
#include <tge/net/stream_socket.hpp>
#include <tge/net/udp_socket.hpp>
#include <tge/threading/watch_id.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_set>

namespace Lkt::Net
{
// A master talks on a socket of its own, a connected datagram socket or a stream, so what it sends can never be taken
// for another's. Only the socket its transport needs is opened.
struct SMasterConversation final
{
	SConversationRecord record;
	Query::SServerAddress address;
	Tge::Net::CUdpSocket datagramSocket;
	Tge::Net::CStreamSocket streamSocket;
	std::optional<Tge::Threading::SWatchId> watch;
	std::unordered_set<uint64_t> listed;
	size_t numStreamBytes{ 0 };
	bool isStream{ false };
};
} // namespace Lkt::Net
