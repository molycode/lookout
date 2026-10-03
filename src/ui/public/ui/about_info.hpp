#pragma once

#include <string_view>

namespace Lkt::Ui
{
struct SAboutInfo final
{
	std::string_view version;
	std::string_view configDir;
	std::string_view logsDir;
};
} // namespace Lkt::Ui
