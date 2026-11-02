#include "config/version_check.hpp"
#include "loggers.hpp"
#include <tge/io/write_file_atomically.hpp>
#include <expected>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace Lkt::Config
{
namespace
{
// tools/windows/lookout.nsi reads and writes this name too.
constexpr std::string_view MarkerName{ "version-check-off" };
} // namespace

//////////////////////////////////////////////////////////////////////////
// A choice that cannot be read keeps the check off: nothing is asked that the user may have declined.
bool IsVersionCheckOn(std::string_view configDir)
{
	bool isOn{ true };

	if (!configDir.empty())
	{
		std::filesystem::path const marker{ std::filesystem::path{ configDir } / MarkerName };
		std::error_code error{};
		bool const isOff{ std::filesystem::exists(marker, error) };

		isOn = !isOff && error.value() == 0;

		if (error.value() != 0)
		{
			gLog.Warning("Cannot tell whether the version check was turned off, so it stays off: '{}': {}", marker.string(), error.message());
		}
	}

	return isOn;
}

//////////////////////////////////////////////////////////////////////////
bool SetVersionCheck(std::string_view configDir, bool isOn)
{
	std::string_view const state{ isOn ? "on" : "off" };
	bool isKept{ false };

	if (!configDir.empty())
	{
		std::filesystem::path const marker{ std::filesystem::path{ configDir } / MarkerName };
		std::error_code error{};

		if (isOn)
		{
			std::filesystem::remove(marker, error);
		}
		else
		{
			std::expected<void, std::error_code> const written{ Tge::IO::WriteFileAtomically(marker, {}) };

			error = written.has_value() ? std::error_code{} : written.error();
		}

		isKept = error.value() == 0;

		if (!isKept)
		{
			gLog.Warning("The version check stays {} only until Lookout quits: cannot {} '{}': {}", state, isOn ? "remove" : "write", marker.string(), error.message());
		}
	}
	else
	{
		gLog.Warning("The version check stays {} only until Lookout quits: there is no config directory", state);
	}

	return isKept;
}
} // namespace Lkt::Config
