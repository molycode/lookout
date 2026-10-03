#include "server_mods.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace Lkt::Browser
{
namespace
{
//////////////////////////////////////////////////////////////////////////
SServerEntry MakeEntry(EServerState state, std::string mod)
{
	SServerEntry entry{};

	entry.state = state;
	entry.summary.mod = std::move(mod);

	return entry;
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerMods, DistinctModsAreSortedIgnoringCase)
{
	std::vector<SServerEntry> const entries{ MakeEntry(EServerState::Online, "omnibot"), MakeEntry(EServerState::Online, "Legacy"),
		MakeEntry(EServerState::Online, "legacy"), MakeEntry(EServerState::Online, "jaymod") };
	std::vector<std::string> mods{};

	CollectMods(entries, mods);

	ASSERT_EQ(mods.size(), 3u);
	EXPECT_EQ(mods[0], "jaymod");
	EXPECT_TRUE(mods[1] == "Legacy" || mods[1] == "legacy");
	EXPECT_EQ(mods[2], "omnibot");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerMods, KeptSpellingDoesNotDependOnAnswerOrder)
{
	std::vector<SServerEntry> const entries{ MakeEntry(EServerState::Online, "legacy"), MakeEntry(EServerState::Online, "Legacy") };
	std::vector<SServerEntry> const reversed{ entries.rbegin(), entries.rend() };
	std::vector<std::string> mods{};
	std::vector<std::string> reversedMods{};

	CollectMods(entries, mods);
	CollectMods(reversed, reversedMods);

	EXPECT_EQ(mods, reversedMods);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerMods, SilentServersAndEmptyModsAreLeftOut)
{
	std::vector<SServerEntry> const entries{ MakeEntry(EServerState::NoAnswer, "nitmod"), MakeEntry(EServerState::Online, ""),
		MakeEntry(EServerState::Online, "legacy") };
	std::vector<std::string> mods{};

	CollectMods(entries, mods);

	EXPECT_EQ(mods, std::vector<std::string>{ "legacy" });
}
} // namespace
} // namespace Lkt::Browser
