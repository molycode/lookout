#pragma once

#include <cstdint>

namespace Lkt::Games
{
enum class EGameFieldKind : uint8_t
{
	Version,
	Text,
	Characters,
	Number,
	Choice,
	Protocol,
	ProtocolOptions,
	TextList,
	Group,
	GroupList
};
} // namespace Lkt::Games
