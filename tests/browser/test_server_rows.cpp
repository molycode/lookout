#include "server_rows.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Lkt::Browser
{
namespace
{
using Rows = std::vector<uint32_t>;

//////////////////////////////////////////////////////////////////////////
SServerEntry MakeOnline(uint32_t host, std::string name, uint32_t numPlayers, uint32_t pingMs)
{
	SServerEntry entry{};

	entry.address = Query::SServerAddress{ 0x0A000000 + host, 27960 };
	entry.state = EServerState::Online;
	entry.pingMs = pingMs;
	entry.summary.name.plain = std::move(name);
	entry.summary.map = "q3dm17";
	entry.summary.mod = "baseq3";
	entry.summary.mode = "Free for All";
	entry.summary.numPlayers = numPlayers;
	entry.summary.maxPlayers = 16;
	entry.searchText = entry.summary.name.plain + "\nq3dm17\nbaseq3\nFree for All";

	return entry;
}

//////////////////////////////////////////////////////////////////////////
SServerEntry MakeSilent(uint32_t host, EServerState state, bool isFavourite)
{
	SServerEntry entry{};

	entry.address = Query::SServerAddress{ 0x0A000000 + host, 27960 };
	entry.state = state;
	entry.isFavourite = isFavourite;

	return entry;
}

//////////////////////////////////////////////////////////////////////////
Rows Build(std::vector<SServerEntry> const& entries, Config::SServerFilter const& filter = {}, Config::SSortOrder const& sort = {})
{
	Rows rows{};

	BuildRows(entries, filter, sort, rows);

	return rows;
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, FavouriteShowsInEveryStateAndDespiteEveryFilter)
{
	std::vector<SServerEntry> entries{ MakeSilent(1, EServerState::Pending, true), MakeSilent(2, EServerState::NoAnswer, true),
		MakeSilent(3, EServerState::BadReply, true), MakeOnline(4, "Empty", 0, 900) };
	Config::SServerFilter filter{};

	entries[3].isFavourite = true;
	filter.search = "nothing matches this";
	filter.showEmpty = false;
	filter.maxPingMs = 50;
	filter.mod = "osp";

	EXPECT_EQ(Build(entries, filter).size(), 4u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, SilentServersAreHidden)
{
	std::vector<SServerEntry> const entries{ MakeSilent(1, EServerState::Pending, false), MakeSilent(2, EServerState::NoAnswer, false),
		MakeSilent(3, EServerState::BadReply, false), MakeOnline(4, "Answered", 1, 40) };

	EXPECT_EQ(Build(entries), (Rows{ 3 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, SearchFindsAPlayerIgnoringCase)
{
	std::vector<SServerEntry> entries{ MakeOnline(1, "Arena", 1, 40), MakeOnline(2, "Other", 1, 40) };
	Config::SServerFilter filter{};

	entries[0].searchText += "\nBigJoe";
	filter.search = "bIGjOE";

	EXPECT_EQ(Build(entries, filter), (Rows{ 0 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, SearchCannotSpanTwoFields)
{
	std::vector<SServerEntry> const entries{ MakeOnline(1, "Arena", 1, 40) };
	Config::SServerFilter filter{};

	filter.search = "naq3";

	EXPECT_TRUE(Build(entries, filter).empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, EmptyServersCanBeHidden)
{
	std::vector<SServerEntry> const entries{ MakeOnline(1, "Empty", 0, 40), MakeOnline(2, "Busy", 1, 40) };
	Config::SServerFilter filter{};

	filter.showEmpty = false;

	EXPECT_EQ(Build(entries, filter), (Rows{ 1 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, FullServersCanBeHidden)
{
	std::vector<SServerEntry> const entries{ MakeOnline(1, "Full", 16, 40), MakeOnline(2, "Room", 15, 40) };
	Config::SServerFilter filter{};

	filter.showFull = false;

	EXPECT_EQ(Build(entries, filter), (Rows{ 1 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, PingLimitHidesOnlySlowerServers)
{
	std::vector<SServerEntry> const entries{ MakeOnline(1, "AtLimit", 1, 100), MakeOnline(2, "Slow", 1, 101) };
	Config::SServerFilter filter{};

	filter.maxPingMs = 100;

	EXPECT_EQ(Build(entries, filter), (Rows{ 0 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, ModFilterIgnoresCase)
{
	std::vector<SServerEntry> entries{ MakeOnline(1, "Osp", 1, 40), MakeOnline(2, "Vanilla", 1, 40) };
	Config::SServerFilter filter{};

	entries[0].summary.mod = "OSP";
	filter.mod = "osp";

	EXPECT_EQ(Build(entries, filter), (Rows{ 0 }));
}

//////////////////////////////////////////////////////////////////////////
// The second entry has the lower address, so only the column can put the first one ahead.
TEST(ServerRows, EveryColumnSortsBothWays)
{
	std::vector<SServerEntry> entries{ MakeOnline(9, "alpha", 1, 10), MakeOnline(1, "Beta", 2, 20) };

	entries[0].summary.map = "a1";
	entries[0].summary.mod = "a";
	entries[0].summary.mode = "Capture";
	entries[1].summary.map = "B2";
	entries[1].summary.mod = "B";
	entries[1].summary.mode = "deathmatch";
	entries[1].isFavourite = true;
	entries[1].summary.hasPassword = true;

	for (size_t column{ 0 }; column < Config::NumSortColumns; ++column)
	{
		SCOPED_TRACE(column);

		Config::ESortColumn const sortColumn{ static_cast<Config::ESortColumn>(column) };

		EXPECT_EQ(Build(entries, {}, Config::SSortOrder{ sortColumn, true }), (Rows{ 0, 1 }));
		EXPECT_EQ(Build(entries, {}, Config::SSortOrder{ sortColumn, false }), (Rows{ 1, 0 }));
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, OnlineRowsComeFirstBothWays)
{
	std::vector<SServerEntry> const entries{ MakeSilent(1, EServerState::NoAnswer, true), MakeOnline(2, "Answered", 1, 50) };

	EXPECT_EQ(Build(entries, {}, Config::SSortOrder{ Config::ESortColumn::Ping, true }), (Rows{ 1, 0 }));
	EXPECT_EQ(Build(entries, {}, Config::SSortOrder{ Config::ESortColumn::Ping, false }), (Rows{ 1, 0 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, UnansweredFavouriteLeadsWhenSortedByFavourite)
{
	std::vector<SServerEntry> const entries{ MakeOnline(1, "Answered", 1, 50), MakeSilent(2, EServerState::NoAnswer, true) };

	EXPECT_EQ(Build(entries, {}, Config::SSortOrder{ Config::ESortColumn::Favourite, false }), (Rows{ 1, 0 }));
}

//////////////////////////////////////////////////////////////////////////
// The lower address has fewer players, so only the player count can put the fuller server ahead.
TEST(ServerRows, GroupedServersKeepMorePlayersFirstBothWays)
{
	std::vector<SServerEntry> entries{ MakeOnline(1, "Other", 3, 50), MakeOnline(2, "Few", 1, 50), MakeOnline(3, "Many", 5, 50) };

	entries[1].isFavourite = true;
	entries[1].summary.hasPassword = true;
	entries[2].isFavourite = true;
	entries[2].summary.hasPassword = true;

	for (Config::ESortColumn const column : { Config::ESortColumn::Favourite, Config::ESortColumn::Password })
	{
		SCOPED_TRACE(static_cast<int>(column));

		EXPECT_EQ(Build(entries, {}, Config::SSortOrder{ column, false }), (Rows{ 2, 1, 0 }));
		EXPECT_EQ(Build(entries, {}, Config::SSortOrder{ column, true }), (Rows{ 0, 2, 1 }));
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, UnansweredServerTrailsWhenSortedByPassword)
{
	std::vector<SServerEntry> entries{ MakeOnline(1, "Locked", 1, 50), MakeSilent(2, EServerState::NoAnswer, true) };

	entries[0].summary.hasPassword = true;

	EXPECT_EQ(Build(entries, {}, Config::SSortOrder{ Config::ESortColumn::Password, true }), (Rows{ 0, 1 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerRows, EqualNamesFallBackToTheAddressAscendingEvenDescending)
{
	std::vector<SServerEntry> const entries{ MakeOnline(2, "Same", 1, 40), MakeOnline(1, "same", 1, 40) };

	EXPECT_EQ(Build(entries, {}, Config::SSortOrder{ Config::ESortColumn::Name, false }), (Rows{ 1, 0 }));
}
} // namespace
} // namespace Lkt::Browser
