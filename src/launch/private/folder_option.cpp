#include "launch/folder_option.hpp"
#include "install_folder.hpp"
#include "loggers.hpp"
#include "query/game_definition.hpp"
#include <tge/platform.hpp>
#include <filesystem>
#include <string>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
// The program game.json names is Linux's.
bool IsFolderInstallSupported(Query::SGameDefinition const& game)
{
#if defined(TGE_PLATFORM_WINDOWS)
	return false;
#elif defined(TGE_PLATFORM_LINUX)
	return !game.launch.onLinux.program.empty();
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
}

//////////////////////////////////////////////////////////////////////////
std::expected<SLaunchOption, ELaunchError> MakeFolderOption(Query::SGameDefinition const& game, std::string_view folder)
{
	std::expected<SLaunchOption, ELaunchError> result{ std::unexpected{ ELaunchError::FolderInstallUnsupported } };

	if (IsFolderInstallSupported(game))
	{
		std::expected<std::filesystem::path, std::string> const program{ CheckInstallFolder(game.launch.onLinux, std::filesystem::path{ folder }) };

		if (program.has_value())
		{
			result = SLaunchOption{ {}, {}, {}, { program->string() }, std::string{ folder } };
		}
		else
		{
			gLog.Warning("'{}' cannot start {}: {}", folder, game.name, program.error());
			result = std::unexpected{ ELaunchError::BrokenInstallFolder };
		}
	}

	return result;
}
} // namespace Lkt::Launch
