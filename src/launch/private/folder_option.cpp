#include "launch/folder_option.hpp"
#include "install_folder.hpp"
#include "launch_hints.hpp"
#include "loggers.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <filesystem>
#include <string>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
bool IsFolderInstallSupported(Query::EGame game)
{
	return !GetLaunchHints(game).program.empty();
}

//////////////////////////////////////////////////////////////////////////
std::expected<SLaunchOption, ELaunchError> MakeFolderOption(Query::EGame game, std::string_view folder)
{
	std::expected<SLaunchOption, ELaunchError> result{ std::unexpected{ ELaunchError::FolderInstallUnsupported } };
	SLaunchHints const& hints{ GetLaunchHints(game) };

	if (IsFolderInstallSupported(game))
	{
		std::expected<std::filesystem::path, std::string> const program{ CheckInstallFolder(hints, std::filesystem::path{ folder }) };

		if (program.has_value())
		{
			result = SLaunchOption{ {}, {}, {}, { program->string() }, std::string{ folder } };
		}
		else
		{
			gLog.Warning("'{}' cannot start {}: {}", folder, Query::GetGame(game).name, program.error());
			result = std::unexpected{ ELaunchError::BrokenInstallFolder };
		}
	}

	return result;
}
} // namespace Lkt::Launch
