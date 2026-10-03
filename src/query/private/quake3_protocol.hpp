#pragma once

#include "query/protocol.hpp"

namespace Lkt::Query
{
class CQuake3Protocol final : public IProtocol
{
public:

	CQuake3Protocol() = default;
	~CQuake3Protocol() override = default;

	// Lkt::Query::IProtocol
	std::vector<std::byte> MasterRequest(SGameDefinition const& game) const override;
	std::expected<void, EParseError> ParseMasterReply(std::span<std::byte const> datagram, std::vector<SServerAddress>& servers) const override;
	std::vector<std::byte> StatusRequest() const override;
	std::expected<SStatusReply, EParseError> ParseStatusReply(std::span<std::byte const> datagram) const override;
	// ~Lkt::Query::IProtocol
};
} // namespace Lkt::Query
