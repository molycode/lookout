#include "download/lookout_games.hpp"
#include <format>

namespace Lkt::Download
{
//////////////////////////////////////////////////////////////////////////
SDownloadSource GetLookoutGamesSource(std::string_view version)
{
	SDownloadSource source{};

	source.origin.host = "raw.githubusercontent.com";
	source.origin.userAgent = std::format("Lookout/{} (+https://github.com/molycode/lookout)", version);
	source.repository = "/molycode/lookout-games";

	return source;
}
} // namespace Lkt::Download
