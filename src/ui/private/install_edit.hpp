#pragma once

#include <cstdint>
#include <string>

namespace Lkt::Ui
{
struct SInstallEdit final
{
	uint32_t id{ 0 };
	std::string name;
	std::string command;
};
} // namespace Lkt::Ui
