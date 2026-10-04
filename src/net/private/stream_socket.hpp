#pragma once

#include "query/server_address.hpp"
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <expected>
#include <span>

namespace Lkt::Net
{
// A non-blocking IPv4 TCP connection. Errors are errno values; a failed connect surfaces as an error from Receive once
// the descriptor is readable, and EAGAIN from Receive means nothing is waiting.
class CStreamSocket final : private Tge::SNoCopyNoMove
{
public:

	CStreamSocket() = default;
	~CStreamSocket() = default;

	bool Initialize();
	void Terminate();

	// A connection still being made counts as made.
	std::expected<void, int> Connect(Query::SServerAddress const& peer) const;
	// A partial write reports EWOULDBLOCK: on a non-blocking socket only a full send buffer cuts one short.
	std::expected<void, int> Send(std::span<std::byte const> bytes) const;
	// Zero bytes when the peer closed the connection.
	std::expected<size_t, int> Receive(std::span<std::byte> buffer) const;

	int GetDescriptor() const;

private:

	int m_descriptor{ -1 };
};
} // namespace Lkt::Net
