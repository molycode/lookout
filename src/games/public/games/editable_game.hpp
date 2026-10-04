#pragma once

#include <string>

namespace Lkt::Games
{
// The problem says why the user's file could not be opened; the text is then the downloaded one, or empty.
struct SEditableGame final
{
	std::string text;
	std::string problem;
};
} // namespace Lkt::Games
