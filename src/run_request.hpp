#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Lkt
{
namespace Query
{
struct SGameDefinition;
} // namespace Query

// What Lookout was started to do: list a game's servers, download games, or else open its window.
struct SRunRequest final
{
	Query::SGameDefinition const* pListGame{ nullptr };
	// Set for --download; no keys means every game that is missing or has an update.
	std::optional<std::vector<std::string>> downloadKeys;
};
} // namespace Lkt
