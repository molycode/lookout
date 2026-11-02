#include "launch/folder_option.hpp"
#include "install_folder.hpp"
#include "loggers.hpp"
#include "query/game_definition.hpp"
#include <tge/platform.hpp>
#include <filesystem>
#include <string>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
// The program and the files that show the game is there, as this platform's launch facts name them.
auto const& GetFolderFacts(Query::SGameDefinition const& game)
{
#if defined(TGE_PLATFORM_WINDOWS)
	return game.launch.onWindows;
#elif defined(TGE_PLATFORM_LINUX)
	return game.launch.onLinux;
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool IsFolderInstallSupported(Query::SGameDefinition const& game)
{
	return !GetFolderFacts(game).program.empty();
}

//////////////////////////////////////////////////////////////////////////
std::expected<SLaunchOption, ELaunchError> MakeFolderOption(Query::SGameDefinition const& game, std::string_view folder)
{
	std::expected<SLaunchOption, ELaunchError> result{ std::unexpected{ ELaunchError::FolderInstallUnsupported } };

	if (IsFolderInstallSupported(game))
	{
		std::expected<std::filesystem::path, std::string> const program{ CheckInstallFolder(std::filesystem::path{ folder }, GetFolderFacts(game).program,
			GetFolderFacts(game).requiredFiles) };

		if (program.has_value())
		{
			std::filesystem::path preferred{ *program };

			preferred.make_preferred();
			result = MakeInstallOption({}, {}, {}, preferred);
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
