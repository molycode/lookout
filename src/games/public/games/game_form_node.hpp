#pragma once

#include <string>
#include <vector>

namespace Lkt::Games
{
struct SGameFormNode final
{
	std::string name;
	std::string text;
	std::string note;
	std::vector<std::string> texts;
	std::vector<SGameFormNode> children;
	bool isPresent{ false };

	friend bool operator==(SGameFormNode const&, SGameFormNode const&) = default;
};
} // namespace Lkt::Games
