#include "gog_source.hpp"
#include "install_folder.hpp"
#include "launch/launcher_ids.hpp"
#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
void CGogSource::Initialize(SLaunchEnvironment const& environment)
{
	m_pEnvironment = &environment;
}

//////////////////////////////////////////////////////////////////////////
void CGogSource::Terminate()
{
	m_pEnvironment = nullptr;
}

//////////////////////////////////////////////////////////////////////////
std::vector<SLaunchOption> CGogSource::Find(Query::SLaunchHints const& hints, std::string_view gameName) const
{
	Query::SWindowsLaunchHints const& facts{ hints.onWindows };
	std::vector<SGogInstall> const& installs{ m_pEnvironment->gogInstalls };
	auto const install{ std::ranges::find(installs, facts.gogId, &SGogInstall::id) };
	std::vector<SLaunchOption> options{};

	if (facts.gogId != 0 && install != installs.end())
	{
		std::optional<std::filesystem::path> const program{ FindInstallProgram(install->folder, facts.program, facts.requiredFiles, gameName) };

		if (program.has_value())
		{
			options.emplace_back(MakeInstallOption(std::string{ GogLauncherId }, install->name.empty() ? std::string{ gameName } : install->name,
				install->folder.string(), *program));
		}
	}

	return options;
}
} // namespace Lkt::Launch
