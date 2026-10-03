#include "launch_hints.hpp"
#include <tge/assert.hpp>
#include <array>
#include <cstddef>

namespace Lkt::Launch
{
namespace
{
// Only names seen on real installs; Steam's Kingpin.desktop runs a steam:// URL, which drops the connect arguments.
constexpr std::array<std::string_view, 1> KingpinDesktopFiles{ "kingpin-native.desktop" };
constexpr std::array<std::string_view, 2> KingpinRequiredFiles{ "kingpin.x86", "main/pak0.pak" };
constexpr std::array<std::string_view, 1> RtcwDesktopFiles{ "id-linux-rtcw-mp.desktop" };
constexpr std::array<std::string_view, 2> RtcwRequiredFiles{ "kit/id-run", "rtcw/.installed-rtcw-mp" };

constexpr std::array<SLaunchHints, Query::NumGames> Hints
{
	SLaunchHints{ Query::EGame::Kingpin, KingpinDesktopFiles, "Games/Kingpin", "run-game.sh", KingpinRequiredFiles },
	SLaunchHints{ Query::EGame::Quake2, {}, {}, {}, {} },
	SLaunchHints{ Query::EGame::RtcwMultiplayer, RtcwDesktopFiles, "Games/id-linux", "rtcw-mp", RtcwRequiredFiles },
	SLaunchHints{ Query::EGame::EnemyTerritory, {}, {}, {}, {} },
	SLaunchHints{ Query::EGame::Quake3, {}, {}, {}, {} }
};

//////////////////////////////////////////////////////////////////////////
constexpr bool IsIndexedByGame()
{
	bool indexed{ true };

	for (size_t index{ 0 }; index < Hints.size(); ++index)
	{
		indexed = indexed && static_cast<size_t>(Hints[index].game) == index;
	}

	return indexed;
}

static_assert(IsIndexedByGame(), "GetLaunchHints indexes the hints by EGame, so the entries must follow its order");
} // namespace

//////////////////////////////////////////////////////////////////////////
SLaunchHints const& GetLaunchHints(Query::EGame game)
{
	size_t const index{ static_cast<size_t>(game) };

	TGE_ASSERT(index < Hints.size(), "EGame value outside the launch hints");

	return Hints[index];
}
} // namespace Lkt::Launch
