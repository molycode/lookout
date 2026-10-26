#pragma once

#include "download/offer_state.hpp"
#include <string>

namespace Lkt::Download
{
struct SProtocolOffer final
{
	std::string name;
	EOfferState state{ EOfferState::NotInstalled };
	std::string problem;
};
} // namespace Lkt::Download
