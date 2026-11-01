#include "loader_input.hpp"
#include <cstddef>

namespace Lkt::Games
{
//////////////////////////////////////////////////////////////////////////
// Whether a change at this path, relative to the user folder, can change what the loader reads: a game's files, a
// protocol, the folders holding them, and downloaded itself (a download reloads what it changes inside). Dot entries
// are left out, as the loader leaves them: editors' swap files and a checkout's .git change all the time. On Windows
// the settings file sits beside the games, and it must not reload them.
bool IsLoaderInput(std::filesystem::path const& relative)
{
	std::filesystem::path first{};
	size_t numParts{ 0 };
	bool hasDotPart{ false };

	for (std::filesystem::path const& part : relative)
	{
		std::filesystem::path::string_type const& name{ part.native() };

		hasDotPart = hasDotPart || (!name.empty() && name.front() == '.');
		first = (numParts == 0) ? part : first;
		++numParts;
	}

	bool const isGames{ first == "games" };
	bool const isProtocols{ first == "protocols" };

	return !hasDotPart && ((numParts == 1 && (isGames || isProtocols || first == "downloaded")) || (numParts == 2 && (isGames || isProtocols))
		|| (numParts == 3 && isGames));
}
} // namespace Lkt::Games
