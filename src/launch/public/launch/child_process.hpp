#pragma once

#include <string>
#include <sys/types.h>

namespace Lkt::Launch
{
struct SChildProcess final
{
	pid_t pid{ 0 };
	std::string gameName;
	std::string logPath;
};
} // namespace Lkt::Launch
