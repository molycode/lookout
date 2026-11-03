#include "launch/launch_error.hpp"
#include <tge/platform.hpp>

namespace Lkt::Launch
{
namespace
{
#if defined(TGE_PLATFORM_WINDOWS)
constexpr std::string_view BadCommandText{ "the command cannot be split: close every quote, and put any argument holding a space or one of & | < > ^ % in "
	"double quotes, writing \\\" for a quote inside them" };
constexpr std::string_view BadParametersText{ "the parameters cannot be split: close every quote, and put any parameter holding a space or one of & | < > ^ % "
	"in double quotes, writing \\\" for a quote inside them" };
#elif defined(TGE_PLATFORM_LINUX)
constexpr std::string_view BadCommandText{ "the command cannot be split: close every quote, put any argument holding a space or one of ' ~ $ ` ; | & < > ( ) * ? # "
	"in double quotes, and inside them write \\ for a backslash" };
constexpr std::string_view BadParametersText{ "the parameters cannot be split: close every quote, put any parameter holding a space or one of ' ~ $ ` ; | & < > "
	"( ) * ? # in double quotes, and inside them write \\ for a backslash" };
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
} // namespace

//////////////////////////////////////////////////////////////////////////
std::string_view ToString(ELaunchError error)
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
			text = BadCommandText;
			break;
		case ELaunchError::BadParameters:
			text = BadParametersText;
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
		case ELaunchError::ProgramNotFound:
			text = "the program cannot be found";
			break;
		case ELaunchError::ProgramNotInPath:
			text = "no folder in PATH holds the program; give its full path";
			break;
		case ELaunchError::ProgramNotRunnable:
			text = "the file is not a program Lookout can start";
			break;
		case ELaunchError::ProgramAccessDenied:
			text = "permission to start the program is denied";
			break;
		case ELaunchError::ProgramNeedsAdministrator:
			text = "the program asks to run as administrator, which Lookout does not do";
			break;
		case ELaunchError::SpawnFailed:
			text = "the game could not be started";
			break;
	}

	return text;
}
} // namespace Lkt::Launch
