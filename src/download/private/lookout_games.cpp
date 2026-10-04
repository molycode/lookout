#include "download/lookout_games.hpp"

namespace Lkt::Download
{
//////////////////////////////////////////////////////////////////////////
SDownloadSource GetLookoutGamesSource(std::string_view userAgent)
{
	SDownloadSource source{};

	source.origin.host = "raw.githubusercontent.com";
	source.origin.userAgent = userAgent;
	source.repository = "/molycode/lookout-games";

	return source;
}
} // namespace Lkt::Download
