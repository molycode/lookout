#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace Lkt::Script
{
struct SRequestCall final
{
	int function{ 0 };
	std::map<std::string, std::string> const* pOptions{ nullptr };
	std::vector<std::byte> bytes;
	std::string problem;
};
} // namespace Lkt::Script
