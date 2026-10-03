#pragma once

#include <string>
#include <vector>

namespace Lkt::Launch
{
struct SLaunchOption final
{
	std::string id;
	std::string name;
	std::string location;
	std::vector<std::string> argv;
	std::string workingDir;

	bool operator==(SLaunchOption const&) const = default;
};
} // namespace Lkt::Launch
