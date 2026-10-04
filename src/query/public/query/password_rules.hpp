#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Lkt::Query
{
// What a game's join command line can carry as a password, beyond printable ASCII.
struct SPasswordRules final
{
	uint32_t maxLength{ 0 };
	std::string refusedCharacters;
	std::vector<std::string> refusedSequences;
};
} // namespace Lkt::Query
