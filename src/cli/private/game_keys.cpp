#include "game_keys.hpp"

namespace Lkt::Cli
{
//////////////////////////////////////////////////////////////////////////
std::string FormatGameKeys(std::span<std::string_view const> gameKeys)
{
	std::string keys{};

	for (std::string_view const key : gameKeys)
	{
		keys += keys.empty() ? "" : ", ";
		keys += key;
	}

	return keys.empty() ? std::string{ "none, as no game is installed (--download gets them)" } : keys;
}
} // namespace Lkt::Cli
