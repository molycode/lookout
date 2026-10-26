#include "download/lookout_games.hpp"
#include "user_agent.hpp"

namespace Lkt::Download
{
//////////////////////////////////////////////////////////////////////////
SDownloadSource GetLookoutGamesSource(std::string_view version)
{
	SDownloadSource source{};

	source.origin.host = "raw.githubusercontent.com";
	source.origin.userAgent = MakeUserAgent(version);
	source.repository = "/molycode/lookout-games";

	return source;
}
} // namespace Lkt::Download
