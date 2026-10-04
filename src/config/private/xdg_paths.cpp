#include "config/xdg_paths.hpp"
#include <cstdlib>

namespace Lkt::Config
{
namespace
{
//////////////////////////////////////////////////////////////////////////
std::string_view GetEnvironment(char const* pName)
{
	char const* const pValue{ std::getenv(pName) };

	return (pValue != nullptr) ? std::string_view{ pValue } : std::string_view{};
}

//////////////////////////////////////////////////////////////////////////
// The specification ignores a relative value, so a relative override falls back exactly like an unset one.
std::expected<std::filesystem::path, EXdgError> ResolveBaseDir(char const* pVariable, std::string_view homeFallback)
{
	std::expected<std::filesystem::path, EXdgError> result{ std::unexpected{ EXdgError::NoHome } };

	std::filesystem::path const overridden{ GetEnvironment(pVariable) };

	if (overridden.is_absolute())
	{
		result = overridden;
	}
	else
	{
		std::filesystem::path const home{ GetEnvironment("HOME") };

		if (home.is_absolute())
		{
			result = home / homeFallback;
		}
	}

	return result;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<std::filesystem::path, EXdgError> GetConfigHome()
{
	return ResolveBaseDir("XDG_CONFIG_HOME", ".config");
}

//////////////////////////////////////////////////////////////////////////
std::expected<std::filesystem::path, EXdgError> GetStateHome()
{
	return ResolveBaseDir("XDG_STATE_HOME", ".local/state");
}

//////////////////////////////////////////////////////////////////////////
std::expected<std::filesystem::path, EXdgError> GetDataHome()
{
	return ResolveBaseDir("XDG_DATA_HOME", ".local/share");
}
} // namespace Lkt::Config
