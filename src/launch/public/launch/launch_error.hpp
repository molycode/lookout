#pragma once

#include <cstdint>
#include <string_view>

namespace Lkt::Launch
{
enum class ELaunchError : uint8_t
{
	NoLauncher,
	ChosenLauncherMissing,
	EmptyCustomCommand,
	BadCustomCommand,
	BrokenInstallFolder,
	FolderInstallUnsupported,
	UnsupportedPassword,
	SpawnFailed
};

std::string_view ToString(ELaunchError error);
} // namespace Lkt::Launch
