#include "query/builtin_games.hpp"

namespace Lkt::Query
{
//////////////////////////////////////////////////////////////////////////
std::vector<SGameDefinition> GetBuiltinGames()
{
	return
	{
		SGameDefinition{
			NoGame, "kingpin", "Kingpin: Life of Crime", EProtocolFamily::Quake2, ETextStyle::Ascii7,
			// master1 and master2.kingpin.info speak only GameSpy's TCP protocol, never the Quake 2 query.
			{ { "master.kingpin.info", 27900 } },
			"",
			// Kingpin's gamename is the mod's own title ("Monkey Mod v2.1").
			{ "hostname", "mapname", "maxclients", "password", { "gamename", "game" } },
			// In order: bagman sets deathmatch as well as teamplay.
			{ { { "coop", "1" }, "Co-op" }, { { "teamplay", "1" }, "Bagman" }, { { "deathmatch", "1" }, "Deathmatch" } },
			{},
			// Only names seen on real installs; Steam's Kingpin.desktop runs a steam:// URL, which drops the connect arguments.
			{ { "kingpin-native.desktop" }, "Games/Kingpin", "run-game.sh", { "kingpin.x86", "main/pak0.pak" } }
		},
		SGameDefinition{
			NoGame, "quake2", "Quake II", EProtocolFamily::Quake2, ETextStyle::Ascii7,
			{ { "master.quakeservers.net", 27900 }, { "master.maraakate.org", 27900 } },
			"",
			// Quake 2 mods often put a version in gamename instead of their title.
			{ "hostname", "mapname", "maxclients", "needpass", { "game", "gamename" } },
			{ { { "ctf", "1" }, "Capture the Flag" }, { { "coop", "1" }, "Co-op" }, { { "deathmatch", "1" }, "Deathmatch" } },
			{},
			{}
		},
		SGameDefinition{
			NoGame, "rtcw", "Return to Castle Wolfenstein", EProtocolFamily::Quake3, ETextStyle::Quake3,
			{ { "wolfmaster.idsoftware.com", 27950 } },
			"60 empty full",
			{ "sv_hostname", "mapname", "sv_maxclients", "g_needpass", { "gamename" } },
			{ { { "g_gametype", "5" }, "Objective" }, { { "g_gametype", "6" }, "Stopwatch" }, { { "g_gametype", "7" }, "Checkpoint" },
				{ { "g_gametype", "8" }, "Capture & Hold" } },
			{},
			{ { "id-linux-rtcw-mp.desktop" }, "Games/id-linux", "rtcw-mp", { "kit/id-run", "rtcw/.installed-rtcw-mp" } }
		},
		SGameDefinition{
			NoGame, "et", "Enemy Territory", EProtocolFamily::Quake3, ETextStyle::EnemyTerritory,
			{ { "etmaster.idsoftware.com", 27950 }, { "etmaster.etlegacy.com", 27950 } },
			"84 empty full",
			{ "sv_hostname", "mapname", "sv_maxclients", "g_needpass", { "gamename" } },
			{ { { "g_gametype", "2" }, "Objective" }, { { "g_gametype", "3" }, "Stopwatch" }, { { "g_gametype", "4" }, "Campaign" },
				{ { "g_gametype", "5" }, "Last Man Standing" }, { { "g_gametype", "6" }, "Map Voting" } },
			{},
			{}
		},
		SGameDefinition{
			NoGame, "quake3", "Quake III Arena", EProtocolFamily::Quake3, ETextStyle::Quake3,
			{ { "master.quake3arena.com", 27950 }, { "master.ioquake3.org", 27950 }, { "dpmaster.deathmask.net", 27950 } },
			"68 empty full",
			{ "sv_hostname", "mapname", "sv_maxclients", "g_needpass", { "gamename" } },
			{ { { "g_gametype", "0" }, "Free for All" }, { { "g_gametype", "1" }, "Tournament" }, { { "g_gametype", "2" }, "Single Player" },
				{ { "g_gametype", "3" }, "Team Deathmatch" }, { { "g_gametype", "4" }, "Capture the Flag" },
				{ { "g_gametype", "5" }, "One Flag CTF" }, { { "g_gametype", "6" }, "Overload" }, { { "g_gametype", "7" }, "Harvester" } },
			{ { "gamename", "q3urt42" }, { "gamename", "q3urt43" }, { "gamename", "baseoa" } },
			{}
		}
	};
}
} // namespace Lkt::Query
