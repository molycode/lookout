#include "browser/server_list.hpp"
#include "geo/countries.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <gtest/gtest.h>
#include <string>

namespace Lkt::Browser
{
namespace
{
constexpr Query::SServerAddress First{ 0x2D5E3A3C, 27960 };
constexpr Query::SServerAddress Second{ 0x2D5E3A3D, 27960 };
constexpr Query::SServerAddress Resolver{ 0x08080808, 27960 };

//////////////////////////////////////////////////////////////////////////
Net::SServerAnswered MakeAnswer(Query::EGame game, Query::SServerAddress const& address, std::string gamename)
{
	Net::SServerAnswered answer{};

	answer.game = game;
	answer.address = address;
	answer.pingMs = 42;
	answer.reply.rules.emplace_back("mapname", "q3dm17");
	answer.reply.rules.emplace_back("gamename", std::move(gamename));

	return answer;
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, ListedServersArePending)
{
	CServerList list{};

	list.Apply(Query::GetGame(Query::EGame::Quake3), Net::SServersListed{ Query::EGame::Quake3, { First } });

	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_EQ(list.GetEntries()[0].state, EServerState::Pending);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, ListingTwiceKeepsOneEntry)
{
	CServerList list{};
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };

	list.Apply(quake3, Net::SServersListed{ Query::EGame::Quake3, { First } });
	list.Apply(quake3, Net::SServersListed{ Query::EGame::Quake3, { First } });

	EXPECT_EQ(list.GetEntries().size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, AnswerBringsTheServerOnline)
{
	CServerList list{};
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };

	list.Apply(quake3, Net::SServersListed{ Query::EGame::Quake3, { First } });
	list.Apply(quake3, MakeAnswer(Query::EGame::Quake3, First, "baseq3"));

	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_EQ(list.GetEntries()[0].state, EServerState::Online);
	EXPECT_EQ(list.GetEntries()[0].pingMs, 42u);
	EXPECT_EQ(list.GetEntries()[0].summary.map, "q3dm17");
	EXPECT_EQ(list.GetNumAnswered(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, ForeignServerIsDropped)
{
	CServerList list{};
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };

	list.Apply(quake3, Net::SServersListed{ Query::EGame::Quake3, { First, Second } });
	list.Apply(quake3, MakeAnswer(Query::EGame::Quake3, First, "q3urt43"));
	list.Apply(quake3, MakeAnswer(Query::EGame::Quake3, Second, "baseq3"));

	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_EQ(list.GetEntries()[0].address, Second);
	EXPECT_EQ(list.GetEntries()[0].state, EServerState::Online);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, SilentServerIsMarked)
{
	CServerList list{};
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };

	list.Apply(quake3, Net::SServersListed{ Query::EGame::Quake3, { First } });
	list.Apply(quake3, Net::SServerFailed{ Query::EGame::Quake3, First, Net::EServerFailure::NoAnswer });

	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_EQ(list.GetEntries()[0].state, EServerState::NoAnswer);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, IgnoresOtherGames)
{
	CServerList list{};

	list.Apply(Query::GetGame(Query::EGame::Quake3), Net::SServersListed{ Query::EGame::Kingpin, { First } });

	EXPECT_TRUE(list.GetEntries().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, FinishedRefreshIsReportedForItsGameOnly)
{
	CServerList list{};
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };

	EXPECT_TRUE(list.Apply(quake3, Net::SRefreshFinished{ Query::EGame::Quake3 }));
	EXPECT_FALSE(list.Apply(quake3, Net::SRefreshFinished{ Query::EGame::Kingpin }));
}

//////////////////////////////////////////////////////////////////////////
// A list with First and Second online, refreshed with the given id.
void Prepare(CServerList& list, uint32_t refreshId)
{
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };

	list.Apply(quake3, Net::SServersListed{ Query::EGame::Quake3, { First, Second } });
	list.Apply(quake3, MakeAnswer(Query::EGame::Quake3, First, "baseq3"));
	list.Apply(quake3, MakeAnswer(Query::EGame::Quake3, Second, "baseq3"));
	list.BeginRefresh(refreshId);
}

//////////////////////////////////////////////////////////////////////////
// Missing counts as neither, so a broken list fails the test instead of crashing it.
bool IsStale(CServerList const& list, Query::SServerAddress const& address)
{
	SServerEntry const* const pEntry{ list.Find(address) };

	return pEntry != nullptr && pEntry->isStale;
}

//////////////////////////////////////////////////////////////////////////
bool IsFresh(CServerList const& list, Query::SServerAddress const& address)
{
	SServerEntry const* const pEntry{ list.Find(address) };

	return pEntry != nullptr && !pEntry->isStale;
}

//////////////////////////////////////////////////////////////////////////
Net::SServersListed MakeListed(Query::SServerAddress const& address, uint32_t refreshId)
{
	return Net::SServersListed{ Query::EGame::Quake3, { address }, refreshId };
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, RefreshMarksEntriesStaleUntilListedAgain)
{
	CServerList list{};

	Prepare(list, 7);
	list.Apply(Query::GetGame(Query::EGame::Quake3), MakeListed(First, 7));

	EXPECT_TRUE(IsFresh(list, First));
	EXPECT_TRUE(IsStale(list, Second));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, FinishDropsServersNotListedAgain)
{
	CServerList list{};
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };

	Prepare(list, 7);
	list.Apply(quake3, MakeListed(First, 7));

	EXPECT_TRUE(list.Apply(quake3, Net::SRefreshFinished{ Query::EGame::Quake3, 7 }));
	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_EQ(list.GetEntries()[0].address, First);
	EXPECT_FALSE(list.GetEntries()[0].isStale);
}

//////////////////////////////////////////////////////////////////////////
// Offline, nothing is listed again, and sweeping would empty the table.
TEST(ServerList, FinishKeepsEverythingWhenEveryMasterFailed)
{
	CServerList list{};
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };

	Prepare(list, 7);

	for (Query::SMasterEndpoint const& master : quake3.masters)
	{
		list.Apply(quake3, Net::SMasterFailed{ Query::EGame::Quake3, std::string{ master.host }, "no route", 7 });
	}

	list.Apply(quake3, Net::SRefreshFinished{ Query::EGame::Quake3, 7 });

	EXPECT_EQ(list.GetNumMastersFailed(), quake3.masters.size());
	EXPECT_TRUE(IsFresh(list, First));
	EXPECT_TRUE(IsFresh(list, Second));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, FinishStillSweepsWhenOnlySomeMastersFailed)
{
	CServerList list{};
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };

	Prepare(list, 7);
	list.Apply(quake3, MakeListed(First, 7));

	for (size_t index{ 1 }; index < quake3.masters.size(); ++index)
	{
		list.Apply(quake3, Net::SMasterFailed{ Query::EGame::Quake3, std::string{ quake3.masters[index].host }, "no route", 7 });
	}

	list.Apply(quake3, Net::SRefreshFinished{ Query::EGame::Quake3, 7 });

	EXPECT_EQ(list.GetEntries().size(), 1u);
	EXPECT_TRUE(IsFresh(list, First));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, ReplacedRefreshFinishNeitherSweepsNorCounts)
{
	CServerList list{};

	Prepare(list, 7);
	list.BeginRefresh(8);

	EXPECT_FALSE(list.Apply(Query::GetGame(Query::EGame::Quake3), Net::SRefreshFinished{ Query::EGame::Quake3, 7 }));
	EXPECT_EQ(list.GetEntries().size(), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, EventsOfAReplacedRefreshAreIgnored)
{
	CServerList list{};
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };
	Query::SServerAddress const third{ 0x2D5E3A3E, 27960 };

	Prepare(list, 8);
	list.Apply(quake3, MakeListed(third, 7));
	list.Apply(quake3, MakeListed(First, 7));

	EXPECT_EQ(list.GetEntries().size(), 2u);
	EXPECT_TRUE(IsStale(list, First));
}

//////////////////////////////////////////////////////////////////////////
// A single server refreshed on its own after the full refresh finished must not empty the table.
TEST(ServerList, StandaloneRefreshFinishDoesNotSweep)
{
	CServerList list{};

	Prepare(list, 7);

	EXPECT_TRUE(list.Apply(Query::GetGame(Query::EGame::Quake3), Net::SRefreshFinished{ Query::EGame::Quake3, 9 }));
	EXPECT_EQ(list.GetEntries().size(), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, FavouriteSurvivesTheSweep)
{
	CServerList list{};

	Prepare(list, 0);
	list.SetFavourite(Second, true);
	list.BeginRefresh(7);
	list.Apply(Query::GetGame(Query::EGame::Quake3), Net::SRefreshFinished{ Query::EGame::Quake3, 7 });

	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_EQ(list.GetEntries()[0].address, Second);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, ForeignFavouriteStaysAsBadReply)
{
	CServerList list{};

	list.SetFavourite(First, true);
	list.Apply(Query::GetGame(Query::EGame::Quake3), MakeAnswer(Query::EGame::Quake3, First, "q3urt43"));

	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_EQ(list.GetEntries()[0].state, EServerState::BadReply);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, NewFavouriteWaitsForItsAnswer)
{
	CServerList list{};

	list.SetFavourite(First, true);

	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_EQ(list.GetEntries()[0].state, EServerState::Pending);
	EXPECT_TRUE(list.GetEntries()[0].isFavourite);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, PlayerNamesAreDecodedBesideThePlayers)
{
	CServerList list{};
	Net::SServerAnswered answer{ MakeAnswer(Query::EGame::Quake3, First, "baseq3") };

	answer.reply.players.emplace_back("^1Big^7Joe", 12, 50);
	answer.reply.players.emplace_back("Ann", 3, 40);
	list.Apply(Query::GetGame(Query::EGame::Quake3), std::move(answer));

	ASSERT_EQ(list.GetEntries().size(), 1u);
	ASSERT_EQ(list.GetEntries()[0].playerNames.size(), 2u);
	EXPECT_EQ(list.GetEntries()[0].playerNames[0].plain, "BigJoe");
	EXPECT_EQ(list.GetEntries()[0].playerNames[1].plain, "Ann");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, SearchTextHoldsDecodedPlayerNames)
{
	CServerList list{};
	Net::SServerAnswered answer{ MakeAnswer(Query::EGame::Quake3, First, "baseq3") };

	answer.reply.players.emplace_back("^1Big^7Joe", 12, 50);
	list.Apply(Query::GetGame(Query::EGame::Quake3), std::move(answer));

	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_TRUE(list.GetEntries()[0].searchText.contains("\nBigJoe"));
}
//////////////////////////////////////////////////////////////////////////
TEST(ServerList, ListedServerGetsItsCountry)
{
	CServerList list{};

	list.Apply(Query::GetGame(Query::EGame::Quake3), Net::SServersListed{ Query::EGame::Quake3, { Resolver } });

	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_EQ(list.GetEntries()[0].country, Geo::FindCountryByCode("US"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerList, SearchTextHoldsTheCountryName)
{
	CServerList list{};
	Query::SGameDefinition const& quake3{ Query::GetGame(Query::EGame::Quake3) };

	list.Apply(quake3, Net::SServersListed{ Query::EGame::Quake3, { Resolver } });
	list.Apply(quake3, MakeAnswer(Query::EGame::Quake3, Resolver, "baseq3"));

	ASSERT_EQ(list.GetEntries().size(), 1u);
	EXPECT_TRUE(list.GetEntries()[0].searchText.contains("\nUnited States of America"));
}
} // namespace
} // namespace Lkt::Browser
