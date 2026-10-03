#include "config/install_ids.hpp"
#include <algorithm>
#include <format>
#include <string_view>

namespace Lkt::Config
{
namespace
{
constexpr std::string_view InstallLauncherPrefix{ "install:" };
} // namespace

//////////////////////////////////////////////////////////////////////////
std::string ToLauncherId(uint32_t installId)
{
	return std::format("{}{}", InstallLauncherPrefix, installId);
}

//////////////////////////////////////////////////////////////////////////
uint32_t NextInstallId(std::span<SGameInstall const> installs)
{
	auto const highest{ std::ranges::max_element(installs, {}, &SGameInstall::id) };
	uint32_t id{ 1 };

	if (highest != installs.end() && highest->id < MaxInstallId)
	{
		id = highest->id + 1;
	}
	else if (highest != installs.end())
	{
		while (FindInstall(installs, id) != nullptr)
		{
			++id;
		}
	}

	return id;
}

//////////////////////////////////////////////////////////////////////////
SGameInstall const* FindInstall(std::span<SGameInstall const> installs, uint32_t id)
{
	auto const it{ std::ranges::find(installs, id, &SGameInstall::id) };

	return (it != installs.end()) ? &*it : nullptr;
}
} // namespace Lkt::Config
