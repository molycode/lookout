#pragma once

#include "query/server_address.hpp"
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <expected>
#include <span>

namespace Lkt::Net
{
// A non-blocking IPv4 datagram socket. Errors are errno values; EAGAIN from Receive means nothing is waiting.
class CUdpSocket final : private Tge::SNoCopyNoMove
{
public:

	CUdpSocket() = default;
	~CUdpSocket() = default;

	bool Initialize();
	void Terminate();

	std::expected<void, int> Send(Query::SServerAddress const& destination, std::span<std::byte const> datagram) const;
	std::expected<size_t, int> Receive(std::span<std::byte> buffer, Query::SServerAddress& source) const;

	int GetDescriptor() const;

private:

	int m_descriptor{ -1 };
};
} // namespace Lkt::Net
