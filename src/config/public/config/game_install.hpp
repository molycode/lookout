#pragma once

#include "config/install_kind.hpp"
#include <cstdint>
#include <string>

namespace Lkt::Config
{
struct SGameInstall final
{
	uint32_t id{ 0 };
	std::string name;
	EInstallKind kind{ EInstallKind::Folder };
	std::string location;

	bool operator==(SGameInstall const&) const = default;
};
} // namespace Lkt::Config
