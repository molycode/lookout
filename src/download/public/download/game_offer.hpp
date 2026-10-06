#pragma once

#include "download/offer_state.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace Lkt::Download
{
struct SGameOffer final
{
	std::string key;
	std::string name;
	EOfferState state{ EOfferState::NotInstalled };
	std::string iconSha256;
	std::string protocol;
	std::optional<uint64_t> protocolVersion{};
	bool isDownloaded{ false };
};
} // namespace Lkt::Download
