#include "query/builtin_games.hpp"
#include <array>

namespace Lkt::Query
{
namespace
{
// master1 and master2.kingpin.info speak only GameSpy's TCP protocol, never the Quake 2 query.
constexpr std::array<SMasterEndpoint, 1> KingpinMasters
{
	SMasterEndpoint{ "master.kingpin.info", 27900 }
};

constexpr std::array<SMasterEndpoint, 2> Quake2Masters
{
	SMasterEndpoint{ "master.quakeservers.net", 27900 },
	SMasterEndpoint{ "master.maraakate.org", 27900 }
};

constexpr std::array<SMasterEndpoint, 1> RtcwMasters
{
	SMasterEndpoint{ "wolfmaster.idsoftware.com", 27950 }
};

constexpr std::array<SMasterEndpoint, 2> EnemyTerritoryMasters
{
	SMasterEndpoint{ "etmaster.idsoftware.com", 27950 },
	SMasterEndpoint{ "etmaster.etlegacy.com", 27950 }
};

constexpr std::array<SMasterEndpoint, 3> Quake3Masters
{
	SMasterEndpoint{ "master.quake3arena.com", 27950 },
	SMasterEndpoint{ "master.ioquake3.org", 27950 },
	SMasterEndpoint{ "dpmaster.deathmask.net", 27950 }
};

// Kingpin's gamename is the mod's own title ("Monkey Mod v2.1"); Quake 2 mods often put a version there instead.
constexpr std::array<std::string_view, 2> KingpinMods{ "gamename", "game" };
constexpr std::array<std::string_view, 2> Quake2Mods{ "game", "gamename" };
constexpr std::array<std::string_view, 1> Quake3FamilyMods{ "gamename" };

// In order: bagman sets deathmatch as well as teamplay.
constexpr std::array<SModeRule, 3> KingpinModes
{
	SModeRule{ { "coop", "1" }, "Co-op" },
	SModeRule{ { "teamplay", "1" }, "Bagman" },
	SModeRule{ { "deathmatch", "1" }, "Deathmatch" }
};

constexpr std::array<SModeRule, 3> Quake2Modes
{
	SModeRule{ { "ctf", "1" }, "Capture the Flag" },
	SModeRule{ { "coop", "1" }, "Co-op" },
	SModeRule{ { "deathmatch", "1" }, "Deathmatch" }
};

constexpr std::array<SModeRule, 4> RtcwModes
{
	SModeRule{ { "g_gametype", "5" }, "Objective" },
	SModeRule{ { "g_gametype", "6" }, "Stopwatch" },
	SModeRule{ { "g_gametype", "7" }, "Checkpoint" },
	SModeRule{ { "g_gametype", "8" }, "Capture & Hold" }
};

constexpr std::array<SModeRule, 5> EnemyTerritoryModes
{
	SModeRule{ { "g_gametype", "2" }, "Objective" },
	SModeRule{ { "g_gametype", "3" }, "Stopwatch" },
	SModeRule{ { "g_gametype", "4" }, "Campaign" },
	SModeRule{ { "g_gametype", "5" }, "Last Man Standing" },
	SModeRule{ { "g_gametype", "6" }, "Map Voting" }
};

constexpr std::array<SModeRule, 8> Quake3Modes
{
	SModeRule{ { "g_gametype", "0" }, "Free for All" },
	SModeRule{ { "g_gametype", "1" }, "Tournament" },
	SModeRule{ { "g_gametype", "2" }, "Single Player" },
	SModeRule{ { "g_gametype", "3" }, "Team Deathmatch" },
	SModeRule{ { "g_gametype", "4" }, "Capture the Flag" },
	SModeRule{ { "g_gametype", "5" }, "One Flag CTF" },
	SModeRule{ { "g_gametype", "6" }, "Overload" },
	SModeRule{ { "g_gametype", "7" }, "Harvester" }
};

constexpr std::array<SKeyMatch, 3> Quake3ForeignServers
{
	SKeyMatch{ "gamename", "q3urt42" },
	SKeyMatch{ "gamename", "q3urt43" },
	SKeyMatch{ "gamename", "baseoa" }
};

// Only names seen on real installs; Steam's Kingpin.desktop runs a steam:// URL, which drops the connect arguments.
constexpr std::array<std::string_view, 1> KingpinDesktopFiles{ "kingpin-native.desktop" };
constexpr std::array<std::string_view, 2> KingpinRequiredFiles{ "kingpin.x86", "main/pak0.pak" };
constexpr std::array<std::string_view, 1> RtcwDesktopFiles{ "id-linux-rtcw-mp.desktop" };
constexpr std::array<std::string_view, 2> RtcwRequiredFiles{ "kit/id-run", "rtcw/.installed-rtcw-mp" };

constexpr std::array<SGameDefinition, 5> BuiltinGames
{
	SGameDefinition{
		NoGame, "kingpin", "Kingpin: Life of Crime", EProtocolFamily::Quake2, ETextStyle::Ascii7,
		KingpinMasters, "", { "hostname", "mapname", "maxclients", "password", KingpinMods }, KingpinModes, {},
		SLaunchHints{ KingpinDesktopFiles, "Games/Kingpin", "run-game.sh", KingpinRequiredFiles }
	},
	SGameDefinition{
		NoGame, "quake2", "Quake II", EProtocolFamily::Quake2, ETextStyle::Ascii7,
		Quake2Masters, "", { "hostname", "mapname", "maxclients", "needpass", Quake2Mods }, Quake2Modes, {}, {}
	},
	SGameDefinition{
		NoGame, "rtcw", "Return to Castle Wolfenstein", EProtocolFamily::Quake3, ETextStyle::Quake3,
		RtcwMasters, "60 empty full", { "sv_hostname", "mapname", "sv_maxclients", "g_needpass", Quake3FamilyMods }, RtcwModes, {},
		SLaunchHints{ RtcwDesktopFiles, "Games/id-linux", "rtcw-mp", RtcwRequiredFiles }
	},
	SGameDefinition{
		NoGame, "et", "Enemy Territory", EProtocolFamily::Quake3, ETextStyle::EnemyTerritory,
		EnemyTerritoryMasters, "84 empty full", { "sv_hostname", "mapname", "sv_maxclients", "g_needpass", Quake3FamilyMods }, EnemyTerritoryModes, {}, {}
	},
	SGameDefinition{
		NoGame, "quake3", "Quake III Arena", EProtocolFamily::Quake3, ETextStyle::Quake3,
		Quake3Masters, "68 empty full", { "sv_hostname", "mapname", "sv_maxclients", "g_needpass", Quake3FamilyMods }, Quake3Modes, Quake3ForeignServers, {}
	}
};
} // namespace

//////////////////////////////////////////////////////////////////////////
std::span<SGameDefinition const> GetBuiltinGames()
{
	return BuiltinGames;
}
} // namespace Lkt::Query
