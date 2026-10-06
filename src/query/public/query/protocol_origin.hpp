#pragma once

#include <cstdint>

namespace Lkt::Query
{
enum class EProtocolOrigin : uint8_t
{
	Downloaded,
	User,
	UserOverDownloaded
};
} // namespace Lkt::Query
