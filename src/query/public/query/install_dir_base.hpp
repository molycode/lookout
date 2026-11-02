#pragma once

#include <cstdint>

namespace Lkt::Query
{
enum class EInstallDirBase : uint8_t
{
	ProgramFiles,
	ProgramFilesX86,
	AppData,
	LocalAppData,
	SystemDrive
};
} // namespace Lkt::Query
