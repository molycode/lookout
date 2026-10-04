#pragma once

#include <cstdint>

namespace Lkt::Fixtures
{
struct SA2sServerSetup final
{
	uint16_t joinPort{ 0 };
	bool isChallenging{ true };
	bool dropsFirstInfo{ false };
	bool sendsRules{ true };
	bool hasPassword{ false };
};
} // namespace Lkt::Fixtures
