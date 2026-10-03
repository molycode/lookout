#pragma once

#include "desktop_entry_error.hpp"
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Launch
{
// String escapes are decoded; Exec still carries its own quoting.
struct SDesktopEntry final
{
	std::string type;
	std::string name;
	std::string exec;
	std::string tryExec;
	std::string path;
	bool isHidden{ false };
};

std::expected<SDesktopEntry, EDesktopEntryError> ParseDesktopEntry(std::string_view text);
} // namespace Lkt::Launch
