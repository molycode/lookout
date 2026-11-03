#include "steam_source.hpp"
#include "install_folder.hpp"
#include "launch/launcher_ids.hpp"
#include "steam_apps.hpp"
#include "query/store_ids.hpp"
#include <filesystem>
#include <optional>
#include <string>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
void CSteamSource::Initialize(SLaunchEnvironment const& environment)
{
	m_pEnvironment = &environment;
}

//////////////////////////////////////////////////////////////////////////
void CSteamSource::Terminate()
{
	m_pEnvironment = nullptr;
}

//////////////////////////////////////////////////////////////////////////
std::vector<SLaunchOption> CSteamSource::Find(Query::SLaunchHints const& hints, std::string_view gameName) const
{
	Query::SWindowsLaunchHints const& facts{ hints.onWindows };
	std::optional<SSteamApp> const app{ facts.program.empty() ? std::nullopt
		: FindInstalledSteamApp(m_pEnvironment->steamLibraries, Query::FindStoreId(hints, Query::SteamStore)) };
	std::optional<std::filesystem::path> const program{ app.has_value() ? FindInstallProgram(app->folder, facts.program, facts.requiredFiles, gameName)
		: std::nullopt };
	std::vector<SLaunchOption> options{};

	if (program.has_value())
	{
		options.emplace_back(MakeInstallOption(std::string{ SteamLauncherId }, app->name.empty() ? std::string{ gameName } : app->name, app->folder.string(),
			*program));
	}

	return options;
}
} // namespace Lkt::Launch
