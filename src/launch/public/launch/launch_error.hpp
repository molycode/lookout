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

constexpr std::string_view ToString(ELaunchError error)
{
	std::string_view text{ "unknown error" };

	switch (error)
	{
		case ELaunchError::NoLauncher:
			text = "no way to start the game was found";
			break;
		case ELaunchError::ChosenLauncherMissing:
			text = "the chosen launcher is no longer there";
			break;
		case ELaunchError::EmptyCustomCommand:
			text = "the command is empty";
			break;
		case ELaunchError::BadCustomCommand:
			text = "the command cannot be split: close every quote, put any argument holding a space or one of ' ~ $ ` ; | & < > ( ) * ? # in double quotes, and inside them write \\ for a backslash";
			break;
		case ELaunchError::BrokenInstallFolder:
			text = "the folder does not hold the game's files";
			break;
		case ELaunchError::FolderInstallUnsupported:
			text = "Lookout does not know how this game starts from a folder";
			break;
		case ELaunchError::UnsupportedPassword:
			text = "the game cannot receive this password on its command line";
			break;
		case ELaunchError::SpawnFailed:
			text = "the game could not be started";
			break;
	}

	return text;
}
} // namespace Lkt::Launch
