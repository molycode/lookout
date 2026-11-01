#pragma once

#include <tge/process/child_process.hpp>
#include <string>

namespace Lkt::Launch
{
struct SChildProcess final
{
	Tge::Process::CChildProcess process{};
	std::string gameName{};
	std::string logPath{};
};
} // namespace Lkt::Launch
