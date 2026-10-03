#pragma once

#include <cstdint>

namespace Lkt::Browser
{
struct SGameStatus final
{
	bool hasRefreshed{ false };
	bool isRefreshing{ false };
	uint32_t numListed{ 0 };
	uint32_t numAnswered{ 0 };
	uint32_t numPlayers{ 0 };
	uint32_t numMastersFailed{ 0 };
};
} // namespace Lkt::Browser
