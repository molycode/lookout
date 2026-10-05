#pragma once

#include "download/offer_state.hpp"
#include <string>

namespace Lkt::Download
{
struct SGameOffer final
{
	std::string key;
	std::string name;
	EOfferState state{ EOfferState::NotInstalled };
	std::string iconSha256;
};
} // namespace Lkt::Download
