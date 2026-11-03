#include "gog_source.hpp"
#include "install_folder.hpp"
#include "launch/launcher_ids.hpp"
#include "query/store_ids.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

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
	std::string_view const id{ Query::FindStoreId(hints, Query::GogStore) };
	uint64_t gogId{ 0 };
	std::from_chars_result const parsed{ std::from_chars(id.data(), id.data() + id.size(), gogId) };
	std::vector<SGogInstall> const& installs{ m_pEnvironment->gogInstalls };
	auto const install{ std::ranges::find(installs, gogId, &SGogInstall::id) };
	std::vector<SLaunchOption> options{};

	if (!id.empty() && parsed.ec == std::errc{} && !facts.program.empty() && install != installs.end())
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
