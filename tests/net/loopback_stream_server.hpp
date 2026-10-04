#pragma once

#include "net/loopback_stream.hpp"
#include "query/server_address.hpp"
#include <tge/non_copyable.hpp>
#include <atomic>
#include <cstdint>
#include <thread>

namespace Lkt::Fixtures
{
// A TCP master on 127.0.0.1, one connection at a time: step by step it waits for the bytes it expects and sends its
// part in writes of the set size; then it closes, or holds the connection until the client or Stop ends it.
class CLoopbackStreamServer final : private Tge::SNoCopyNoMove
{
public:

	CLoopbackStreamServer() = default;
	~CLoopbackStreamServer() = default;

	bool Start(SLoopbackStream stream);
	void Stop();

	Query::SServerAddress GetAddress() const;
	uint32_t GetNumConnections() const;

private:

	void Serve();
	void Converse(int connection);

	int m_descriptor{ -1 };
	uint16_t m_port{ 0 };
	SLoopbackStream m_stream;
	std::atomic<bool> m_isServing{ false };
	std::atomic<uint32_t> m_numConnections{ 0 };
	std::thread m_thread;
};
} // namespace Lkt::Fixtures
