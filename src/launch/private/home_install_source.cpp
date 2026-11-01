#include "home_install_source.hpp"
#include "install_folder.hpp"
#include "launch/home_path.hpp"
#include "launch/launcher_ids.hpp"
#include "loggers.hpp"
#include <expected>
#include <filesystem>
#include <string>
#include <system_error>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
void CHomeInstallSource::Initialize(SLaunchEnvironment const& environment)
{
	m_pEnvironment = &environment;
}

//////////////////////////////////////////////////////////////////////////
void CHomeInstallSource::Terminate()
{
	m_pEnvironment = nullptr;
}

//////////////////////////////////////////////////////////////////////////
std::vector<SLaunchOption> CHomeInstallSource::Find(Query::SLaunchHints const& hints, std::string_view gameName) const
{
	Query::SLinuxLaunchHints const& facts{ hints.onLinux };
	std::vector<SLaunchOption> options{};

	if (!facts.installDir.empty())
	{
		std::filesystem::path const folder{ m_pEnvironment->home / facts.installDir };
		std::error_code error{};

		if (m_pEnvironment->home.empty())
		{
			gLog.Warning("{}'s default folder ~/{} is not checked: HOME is unset or not an absolute path", gameName, facts.installDir);
		}
		else if (std::filesystem::is_directory(folder, error))
		{
			std::expected<std::filesystem::path, std::string> const program{ CheckInstallFolder(folder, facts.program, facts.requiredFiles) };

			if (program.has_value())
			{
				options.emplace_back(std::string{ InstallDirLauncherId }, std::string{ gameName }, ShortenHome(folder.string(), m_pEnvironment->home),
					std::vector<std::string>{ program->string() }, folder.string());
			}
			else
			{
				gLog.Warning("'{}' is not used to start {}: {}", folder.string(), gameName, program.error());
			}
		}
		else if (error.value() != 0 && error != std::errc::no_such_file_or_directory)
		{
			gLog.Warning("Cannot look into '{}' for {}: {}", folder.string(), gameName, error.message());
		}
	}

	return options;
}
} // namespace Lkt::Launch
