#include "net/loopback_https_server.hpp"
#include "sha256.hpp"
#include "download/game_downloads.hpp"
#include "json/json.hpp"
#include "script/script_api.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Lkt::Download
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr std::string_view Commit{ "0123456789abcdef0123456789abcdef01234567" };
constexpr std::chrono::seconds Patience{ 10 };
constexpr uint32_t Loopback{ 0x7F000001 };

//////////////////////////////////////////////////////////////////////////
std::string ReadText(std::filesystem::path const& path)
{
	std::ifstream file{ path, std::ios::binary };

	return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
}

//////////////////////////////////////////////////////////////////////////
JsonValue DescribeFile(std::string const& bytes)
{
	JsonValue file = JsonValue::object();

	file["size"] = bytes.size();
	file["sha256"] = HashSha256(bytes);

	return file;
}

//////////////////////////////////////////////////////////////////////////
// lookout-games' Kingpin and its protocol, served as a repository at /repo, and a data folder to download them into.
class CGameDownloadsTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::filesystem::path const games{ LKT_LOOKOUT_GAMES_DIR };
		std::error_code error{};
		std::string pattern{ (std::filesystem::temp_directory_path(error) / "lookout-downloads-XXXXXX").string() };

		ASSERT_NE(::mkdtemp(pattern.data()), nullptr);
		m_dir = pattern;

		for (std::string_view const file : { "game.json", "icon.png", "icon-licence.txt" })
		{
			m_files[std::format("games/kingpin/{}", file)] = ReadText(games / "games" / "kingpin" / file);
		}

		m_files["protocols/quake2.lua"] = ReadText(games / "protocols" / "quake2.lua");
		ASSERT_TRUE(m_server.Start());
		Serve(MakeIndex());
	}

	void TearDown() override
	{
		std::error_code error{};

		m_downloads.Terminate();
		m_server.Stop();
		std::filesystem::remove_all(m_dir, error);
	}
	// ~testing::Test

	JsonValue MakeIndex() const
	{
		JsonValue index = JsonValue::object();
		JsonValue files = JsonValue::object();

		for (std::string_view const file : { "game.json", "icon.png", "icon-licence.txt" })
		{
			files[std::string{ file }] = DescribeFile(m_files.at(std::format("games/kingpin/{}", file)));
		}

		JsonValue protocol = DescribeFile(m_files.at("protocols/quake2.lua"));

		protocol["api"] = 1;
		index["index"] = 1;
		index["commit"] = Commit;
		index["games"]["kingpin"]["name"] = "Kingpin: Life of Crime";
		index["games"]["kingpin"]["format"] = 1;
		index["games"]["kingpin"]["protocol"] = "quake2";
		index["games"]["kingpin"]["files"] = std::move(files);
		index["protocols"]["quake2"] = std::move(protocol);

		return index;
	}

	// The files under the commit, and the index at main.
	void Serve(JsonValue const& index)
	{
		for (auto const& [path, bytes] : m_files)
		{
			m_server.SetReply(std::format("/repo/{}/{}", Commit, path), Fixtures::SHttpsReply{ .body = bytes });
		}

		m_server.SetReply("/repo/main/index.json", Fixtures::SHttpsReply{ .body = index.dump() });
	}

	// Kingpin again under key, with an icon of its own; Serve the index afterwards.
	void AddGame(JsonValue& index, std::string const& key)
	{
		std::string const icon{ m_files.at("games/kingpin/icon.png") + key };
		JsonValue game = index["games"]["kingpin"];

		game["name"] = key;
		game["files"]["icon.png"] = DescribeFile(icon);
		index["games"][key] = std::move(game);
		m_files[std::format("games/{}/icon.png", key)] = icon;
	}

	void Start()
	{
		SDownloadSource source{};

		source.origin.host = "localhost";
		source.origin.port = m_server.GetPort();
		source.origin.userAgent = "Lookout tests";
		source.origin.caFile = (std::filesystem::path{ LKT_FIXTURES_DIR } / "tls" / "ca.pem").string();
		source.origin.address = Query::SServerAddress{ Loopback, m_server.GetPort() };
		source.repository = "/repo";
		ASSERT_TRUE(m_downloads.Initialize(m_dir, GetCacheDir(), std::move(source), [this]()
		{
			{
				std::lock_guard const lock{ m_mutex };

				m_hasResults = true;
			}

			m_wake.notify_one();
		}));
	}

	// Takes results until isDone; true when the downloaded games changed meanwhile.
	bool RunUntil(std::function<bool()> const& isDone)
	{
		auto const deadline{ std::chrono::steady_clock::now() + Patience };
		bool hasChanged{ m_downloads.Update() };

		while (!isDone() && std::chrono::steady_clock::now() < deadline)
		{
			{
				std::unique_lock lock{ m_mutex };

				m_wake.wait_until(lock, deadline, [this]() { return m_hasResults; });
				m_hasResults = false;
			}

			hasChanged = m_downloads.Update() || hasChanged;
		}

		return hasChanged;
	}

	bool RunUntilIdle()
	{
		bool const hasChanged{ RunUntil([this]() { return m_downloads.GetPhase() == EDownloadPhase::Idle; }) };

		EXPECT_EQ(m_downloads.GetPhase(), EDownloadPhase::Idle);

		return hasChanged;
	}

	void RunUntilIconHeld(std::string_view key)
	{
		std::string const sha256{ GetOffer(key).iconSha256 };

		RunUntil([this, &sha256]() { return !m_downloads.GetIcon(sha256).empty(); });
		EXPECT_FALSE(m_downloads.GetIcon(sha256).empty()) << key;
	}

	std::filesystem::path GetCacheDir() const
	{
		return m_dir / "cache";
	}

	std::filesystem::path GetCachedIconPath(std::string_view key) const
	{
		return GetCacheDir() / "icons" / std::format("{}.png", GetOffer(key).iconSha256);
	}

	void WriteFile(std::filesystem::path const& path, std::string_view bytes) const
	{
		std::error_code error{};

		std::filesystem::create_directories(path.parent_path(), error);

		std::ofstream file{ path, std::ios::binary };

		file << bytes;
	}

	bool IsIconHeld(std::string_view key) const
	{
		return !m_downloads.GetIcon(GetOffer(key).iconSha256).empty();
	}

	void ReadIndex()
	{
		m_downloads.ReadIndex();
		RunUntilIdle();
	}

	bool Download(std::string const& key)
	{
		std::vector<std::string> const keys{ key };

		m_downloads.Download(keys);

		return RunUntilIdle();
	}

	SGameOffer GetOffer(std::string_view key) const
	{
		std::span<SGameOffer const> const offers{ m_downloads.GetOffers() };
		auto const it{ std::ranges::find(offers, key, &SGameOffer::key) };

		EXPECT_NE(it, offers.end()) << key;

		return (it != offers.end()) ? *it : SGameOffer{};
	}

	EOfferState GetState(std::string_view key) const
	{
		std::span<SGameOffer const> const offers{ m_downloads.GetOffers() };
		auto const it{ std::ranges::find(offers, key, &SGameOffer::key) };

		EXPECT_NE(it, offers.end()) << key;

		return (it != offers.end()) ? it->state : EOfferState::NotInstalled;
	}

	std::filesystem::path m_dir;
	std::map<std::string, std::string> m_files;
	Fixtures::CLoopbackHttpsServer m_server;
	CGameDownloads m_downloads;
	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_hasResults{ false };
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, OffersComeFromTheIndex)
{
	Start();
	ReadIndex();

	ASSERT_EQ(m_downloads.GetOffers().size(), 1u);
	EXPECT_EQ(m_downloads.GetOffers().front().name, "Kingpin: Life of Crime");
	EXPECT_EQ(GetState("kingpin"), EOfferState::NotInstalled);
	EXPECT_TRUE(m_downloads.GetProblems().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, OfferNamesItsProtocol)
{
	Start();
	ReadIndex();

	EXPECT_EQ(GetOffer("kingpin").protocol, "quake2");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, OfferCarriesItsProtocolsVersion)
{
	JsonValue index = MakeIndex();

	index["protocols"]["quake2"]["version"] = 9041;
	Serve(index);
	Start();
	ReadIndex();

	EXPECT_EQ(GetOffer("kingpin").protocolVersion, 9041u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, ProtocolWithoutAVersionOffersNone)
{
	Start();
	ReadIndex();

	EXPECT_EQ(GetOffer("kingpin").protocolVersion, std::nullopt);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, ProtocolVersionBelowOneRefusesTheIndex)
{
	JsonValue index = MakeIndex();

	index["protocols"]["quake2"]["version"] = 0;
	Serve(index);
	Start();
	ReadIndex();

	ASSERT_EQ(m_downloads.GetProblems().size(), 1u);
	EXPECT_EQ(m_downloads.GetProblems().front(), "index.json: protocols.quake2.version: must be a whole number from 1");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, DownloadInstallsTheGameAndItsProtocol)
{
	Start();
	ReadIndex();

	EXPECT_TRUE(Download("kingpin"));
	EXPECT_EQ(ReadText(m_dir / "downloaded/games/kingpin/icon.png"), m_files.at("games/kingpin/icon.png"));
	EXPECT_EQ(ReadText(m_dir / "downloaded/protocols/quake2.lua"), m_files.at("protocols/quake2.lua"));
	EXPECT_EQ(GetState("kingpin"), EOfferState::Installed);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, ChangedFileOffersAnUpdate)
{
	Start();
	ReadIndex();
	Download("kingpin");
	m_files["games/kingpin/game.json"] += "\n";
	Serve(MakeIndex());
	ReadIndex();

	EXPECT_EQ(GetState("kingpin"), EOfferState::UpdateAvailable);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, UpdateReplacesTheGame)
{
	Start();
	ReadIndex();
	Download("kingpin");
	m_files["games/kingpin/game.json"] += "\n";
	Serve(MakeIndex());
	ReadIndex();
	Download("kingpin");

	EXPECT_EQ(ReadText(m_dir / "downloaded/games/kingpin/game.json"), m_files.at("games/kingpin/game.json"));
	EXPECT_EQ(GetState("kingpin"), EOfferState::Installed);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, FileThatDiffersFromTheIndexIsNotInstalled)
{
	JsonValue const index = MakeIndex();

	m_files["games/kingpin/icon-licence.txt"][0] ^= 1;
	Serve(index);
	Start();
	ReadIndex();

	Download("kingpin");
	ASSERT_EQ(m_downloads.GetProblems().size(), 1u);
	EXPECT_EQ(m_downloads.GetProblems().front(), "Kingpin: Life of Crime: icon-licence.txt differs from what the index says it is");
	EXPECT_FALSE(std::filesystem::exists(m_dir / "downloaded/games/kingpin"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, NewerFormatNeedsANewerLookout)
{
	JsonValue index = MakeIndex();

	index["games"]["kingpin"]["format"] = 2;
	Serve(index);
	Start();
	ReadIndex();

	EXPECT_EQ(GetState("kingpin"), EOfferState::NeedsNewerLookout);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, NewerScriptApiNeedsANewerLookout)
{
	JsonValue index = MakeIndex();

	index["protocols"]["quake2"]["api"] = Script::ScriptApi + 1;
	Serve(index);
	Start();
	ReadIndex();

	EXPECT_EQ(GetState("kingpin"), EOfferState::NeedsNewerLookout);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, ScriptApiThisLookoutRunsIsOffered)
{
	JsonValue index = MakeIndex();

	index["protocols"]["quake2"]["api"] = Script::ScriptApi;
	Serve(index);
	Start();
	ReadIndex();

	EXPECT_EQ(GetState("kingpin"), EOfferState::NotInstalled);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, RemoveTakesTheGameAndItsProtocol)
{
	std::vector<std::string> const keys{ "kingpin" };

	Start();
	ReadIndex();
	Download("kingpin");
	m_downloads.Remove(keys);

	EXPECT_TRUE(m_downloads.Update());
	EXPECT_FALSE(std::filesystem::exists(m_dir / "downloaded/games/kingpin"));
	EXPECT_FALSE(std::filesystem::exists(m_dir / "downloaded/protocols/quake2.lua"));
	EXPECT_EQ(GetState("kingpin"), EOfferState::NotInstalled);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, RemoveKeepsAProtocolAUserGameNames)
{
	std::vector<std::string> const keys{ "kingpin" };

	std::filesystem::create_directories(m_dir / "games/mygame");
	std::ofstream{ m_dir / "games/mygame/game.json" } << R"json({ "format": 1, "protocol": "quake2" })json";
	Start();
	ReadIndex();
	Download("kingpin");
	m_downloads.Remove(keys);

	EXPECT_TRUE(std::filesystem::exists(m_dir / "downloaded/protocols/quake2.lua"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, GameNoLongerOfferedIsWithdrawn)
{
	JsonValue index = MakeIndex();

	Start();
	ReadIndex();
	Download("kingpin");
	index["games"] = JsonValue::object();
	Serve(index);
	ReadIndex();

	EXPECT_EQ(GetState("kingpin"), EOfferState::Withdrawn);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, GameNotDownloadedIsNotOnDisk)
{
	Start();
	ReadIndex();

	EXPECT_FALSE(GetOffer("kingpin").isDownloaded);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, DownloadedGameThatNeedsANewerLookoutIsStillOnDisk)
{
	JsonValue index = MakeIndex();

	Start();
	ReadIndex();
	Download("kingpin");
	index["games"]["kingpin"]["format"] = 2;
	Serve(index);
	ReadIndex();

	ASSERT_EQ(GetState("kingpin"), EOfferState::NeedsNewerLookout);
	EXPECT_TRUE(GetOffer("kingpin").isDownloaded);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IndexThatIsNotJsonIsAProblem)
{
	m_server.SetReply("/repo/main/index.json", Fixtures::SHttpsReply{ .body = "{ \"index\": " });
	Start();
	ReadIndex();

	ASSERT_EQ(m_downloads.GetProblems().size(), 1u);
	EXPECT_TRUE(m_downloads.GetProblems().front().starts_with("index.json: it is not valid JSON: line 1, column ")) << m_downloads.GetProblems().front();
	EXPECT_FALSE(m_downloads.HasIndex());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, KeyThatIsNoFolderNameRefusesTheIndex)
{
	JsonValue index = MakeIndex();

	index["games"]["../kingpin"] = index["games"]["kingpin"];
	Serve(index);
	Start();
	ReadIndex();

	ASSERT_EQ(m_downloads.GetProblems().size(), 1u);
	EXPECT_EQ(m_downloads.GetProblems().front(), "index.json: games.../kingpin: is not a key: small letters, digits, '-' and '_'");
	EXPECT_TRUE(m_downloads.GetOffers().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IconIsFetchedWhenAskedFor)
{
	Start();
	ReadIndex();
	m_downloads.RequestIcon("kingpin");
	RunUntilIconHeld("kingpin");

	EXPECT_EQ(m_downloads.GetIcon(GetOffer("kingpin").iconSha256), m_files.at("games/kingpin/icon.png"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, ReadingTheIndexFetchesNoIcon)
{
	Start();
	ReadIndex();
	ReadIndex();

	EXPECT_EQ(m_server.GetNumRequests(), 2u);
}

//////////////////////////////////////////////////////////////////////////
// The fetcher answers in order, so a reading asked for after an icon is answered after it.
TEST_F(CGameDownloadsTest, IconThatDiffersFromTheIndexIsNotKept)
{
	JsonValue const index = MakeIndex();

	m_files["games/kingpin/icon.png"][0] ^= 1;
	Serve(index);
	Start();
	ReadIndex();
	m_downloads.RequestIcon("kingpin");
	ReadIndex();

	EXPECT_FALSE(IsIconHeld("kingpin"));
}

//////////////////////////////////////////////////////////////////////////
// An icon asked for later is answered later, so once it is held the earlier ones have been answered.
TEST_F(CGameDownloadsTest, IconThatFailedIsNotAskedForAgain)
{
	JsonValue index = MakeIndex();

	AddGame(index, "second");
	AddGame(index, "third");
	m_files["games/kingpin/icon.png"][0] ^= 1;
	Serve(index);
	Start();
	ReadIndex();
	m_downloads.RequestIcon("kingpin");
	m_downloads.RequestIcon("second");
	RunUntilIconHeld("second");
	m_downloads.RequestIcon("kingpin");
	m_downloads.RequestIcon("third");
	RunUntilIconHeld("third");

	EXPECT_EQ(m_server.GetNumRequests(), 4u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IconThatFailedIsAskedForAgainOnceTheIndexIsRead)
{
	JsonValue const index = MakeIndex();

	m_files["games/kingpin/icon.png"][0] ^= 1;
	Serve(index);
	Start();
	ReadIndex();
	m_downloads.RequestIcon("kingpin");
	ReadIndex();
	m_downloads.RequestIcon("kingpin");
	ReadIndex();

	EXPECT_EQ(m_server.GetNumRequests(), 5u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IconHeldIsNotFetchedAgain)
{
	Start();
	ReadIndex();
	m_downloads.RequestIcon("kingpin");
	RunUntilIconHeld("kingpin");
	m_downloads.RequestIcon("kingpin");
	ReadIndex();

	EXPECT_EQ(m_server.GetNumRequests(), 3u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IconPendingIsNotAskedForTwice)
{
	Start();
	ReadIndex();
	m_downloads.RequestIcon("kingpin");
	m_downloads.RequestIcon("kingpin");
	ReadIndex();

	EXPECT_EQ(m_server.GetNumRequests(), 3u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, AtMostFourIconsAreFetchedAtOnce)
{
	std::array<std::string_view, 6> const keys{ "kingpin", "second", "third", "fourth", "fifth", "sixth" };
	JsonValue index = MakeIndex();

	for (std::string_view const key : keys | std::views::drop(1))
	{
		AddGame(index, std::string{ key });
	}

	Serve(index);
	Start();
	ReadIndex();

	for (std::string_view const key : keys)
	{
		m_downloads.RequestIcon(key);
	}

	ReadIndex();

	EXPECT_EQ(m_server.GetNumRequests(), 6u);
	EXPECT_EQ(std::ranges::count_if(keys, [this](std::string_view key) { return IsIconHeld(key); }), 4);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IconBeyondTheCapIsFetchedWhenAskedForAgain)
{
	std::array<std::string_view, 6> const keys{ "kingpin", "second", "third", "fourth", "fifth", "sixth" };
	JsonValue index = MakeIndex();

	for (std::string_view const key : keys | std::views::drop(1))
	{
		AddGame(index, std::string{ key });
	}

	Serve(index);
	Start();
	ReadIndex();

	for (std::string_view const key : keys)
	{
		m_downloads.RequestIcon(key);
	}

	RunUntilIconHeld("fourth");

	for (std::string_view const key : keys)
	{
		m_downloads.RequestIcon(key);
	}

	RunUntilIconHeld("sixth");

	EXPECT_TRUE(std::ranges::all_of(keys, [this](std::string_view key) { return IsIconHeld(key); }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, FetchedIconIsKeptInTheCache)
{
	Start();
	ReadIndex();
	m_downloads.RequestIcon("kingpin");
	RunUntilIconHeld("kingpin");

	EXPECT_EQ(ReadText(GetCachedIconPath("kingpin")), m_files.at("games/kingpin/icon.png"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, CachedIconIsNotFetched)
{
	Start();
	ReadIndex();
	WriteFile(GetCachedIconPath("kingpin"), m_files.at("games/kingpin/icon.png"));
	m_downloads.RequestIcon("kingpin");

	EXPECT_TRUE(IsIconHeld("kingpin"));
	EXPECT_EQ(m_server.GetNumRequests(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, CachedIconThatDiffersIsFetchedAgain)
{
	std::string icon{ m_files.at("games/kingpin/icon.png") };

	icon[0] ^= 1;
	Start();
	ReadIndex();
	WriteFile(GetCachedIconPath("kingpin"), icon);
	m_downloads.RequestIcon("kingpin");
	RunUntilIconHeld("kingpin");

	EXPECT_EQ(m_server.GetNumRequests(), 2u);
	EXPECT_EQ(ReadText(GetCachedIconPath("kingpin")), m_files.at("games/kingpin/icon.png"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, CachedIconThatDiffersIsFetchedPastTheCap)
{
	std::array<std::string_view, 5> const keys{ "second", "third", "fourth", "fifth", "kingpin" };
	JsonValue index = MakeIndex();
	std::string icon{ m_files.at("games/kingpin/icon.png") };

	for (std::string_view const key : keys | std::views::take(4))
	{
		AddGame(index, std::string{ key });
	}

	icon[0] ^= 1;
	Serve(index);
	Start();
	ReadIndex();
	WriteFile(GetCachedIconPath("kingpin"), icon);

	for (std::string_view const key : keys)
	{
		m_downloads.RequestIcon(key);
	}

	ReadIndex();

	EXPECT_EQ(m_server.GetNumRequests(), 7u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IconThatCannotBeCachedIsStillHeld)
{
	WriteFile(GetCacheDir(), "not a folder");
	Start();
	ReadIndex();
	m_downloads.RequestIcon("kingpin");
	RunUntilIconHeld("kingpin");

	EXPECT_TRUE(m_downloads.GetProblems().empty());
}

//////////////////////////////////////////////////////////////////////////
// No Update runs between the request and the download, so the icon is still pending when the download asks for it.
TEST_F(CGameDownloadsTest, DownloadAskingForAPendingIconInstallsTheGame)
{
	Start();
	ReadIndex();
	m_downloads.RequestIcon("kingpin");

	EXPECT_TRUE(Download("kingpin"));
	EXPECT_EQ(GetState("kingpin"), EOfferState::Installed);
	EXPECT_TRUE(IsIconHeld("kingpin"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IconAskedForDuringItsGamesDownloadIsHeld)
{
	std::vector<std::string> const keys{ "kingpin" };

	Start();
	ReadIndex();
	m_downloads.Download(keys);
	m_downloads.RequestIcon("kingpin");
	RunUntilIdle();

	EXPECT_EQ(GetState("kingpin"), EOfferState::Installed);
	EXPECT_TRUE(IsIconHeld("kingpin"));
}
} // namespace
} // namespace Lkt::Download
