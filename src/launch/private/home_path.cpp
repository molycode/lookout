#include "launch/home_path.hpp"
#include <format>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
std::string ShortenHome(std::string_view path, std::filesystem::path const& home)
{
	std::string homeText{ home.string() };

	while (homeText.size() > 1 && homeText.ends_with('/'))
	{
		homeText.pop_back();
	}

	bool const isUnderHome{ !homeText.empty() && path.starts_with(homeText)
		&& (path.size() == homeText.size() || path[homeText.size()] == '/') };

	return isUnderHome ? std::format("~{}", path.substr(homeText.size())) : std::string{ path };
}
} // namespace Lkt::Launch
