#include "folder_candidates_source.hpp"
#include "install_folder.hpp"
#include "launch/launcher_ids.hpp"
#include "loggers.hpp"
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <system_error>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
std::filesystem::path GetBaseFolder(SLaunchEnvironment const& environment, Query::EInstallDirBase base)
{
	std::filesystem::path folder{};

	switch (base)
	{
		case Query::EInstallDirBase::ProgramFiles:
			folder = environment.programFiles;
			break;
		case Query::EInstallDirBase::ProgramFilesX86:
			folder = environment.programFilesX86;
			break;
		case Query::EInstallDirBase::AppData:
			folder = environment.appData;
			break;
		case Query::EInstallDirBase::LocalAppData:
			folder = environment.localAppData;
			break;
		case Query::EInstallDirBase::SystemDrive:
			folder = environment.systemDrive;
			break;
	}

	return folder;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CFolderCandidatesSource::Initialize(SLaunchEnvironment const& environment)
{
	m_pEnvironment = &environment;
}

//////////////////////////////////////////////////////////////////////////
void CFolderCandidatesSource::Terminate()
{
	m_pEnvironment = nullptr;
}

//////////////////////////////////////////////////////////////////////////
// A usual folder that does not exist is no news; one that exists but cannot start the game is warned about.
std::vector<SLaunchOption> CFolderCandidatesSource::Find(Query::SLaunchHints const& hints, std::string_view gameName) const
{
	Query::SWindowsLaunchHints const& facts{ hints.onWindows };
	std::vector<SLaunchOption> options{};

	for (size_t index{ 0 }; index < facts.installDirs.size(); ++index)
	{
		std::filesystem::path const base{ GetBaseFolder(*m_pEnvironment, facts.installDirs[index].base) };
		std::filesystem::path folder{ base / facts.installDirs[index].path };
		std::error_code error{};

		folder.make_preferred();

		if (!base.empty() && std::filesystem::is_directory(folder, error))
		{
			std::optional<std::filesystem::path> const program{ FindInstallProgram(folder, facts.program, facts.requiredFiles, gameName) };

			if (program.has_value())
			{
				options.emplace_back(MakeInstallOption(std::format("{}:{}", InstallDirLauncherId, index + 1), std::string{ gameName }, folder.string(), *program));
			}
		}
		else if (!base.empty() && error.value() != 0 && error != std::errc::no_such_file_or_directory)
		{
			gLog.Warning("Cannot look into '{}' for {}: {}", folder.string(), gameName, error.message());
		}
	}

	return options;
}
} // namespace Lkt::Launch
