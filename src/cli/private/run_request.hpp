#pragma once

#include "command.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace Lkt::Cli
{
struct SRunRequest final
{
	ECommand command{ ECommand::Window };
	std::string listGame{};
	std::vector<std::string> downloadKeys{};
	std::filesystem::path checkFolder{};
};
} // namespace Lkt::Cli
