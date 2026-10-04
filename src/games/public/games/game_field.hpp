#pragma once

#include <string_view>

namespace Lkt::Games
{
// A field of game.json; parent is the path of the object that holds it, "masters[]" for each object in masters.
struct SGameField final
{
	std::string_view parent;
	std::string_view name;
	std::string_view description;
};
} // namespace Lkt::Games
