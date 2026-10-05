#include "net/loopback_https_server.hpp"
#include "sha256.hpp"
#include "download/game_downloads.hpp"
#include "json/json.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
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

	void Start(EIndexIcons icons = EIndexIcons::Skip)
	{
		SDownloadSource source{};

		source.origin.host = "localhost";
		source.origin.port = m_server.GetPort();
		source.origin.userAgent = "Lookout tests";
		source.origin.caFile = (std::filesystem::path{ LKT_FIXTURES_DIR } / "tls" / "ca.pem").string();
		source.origin.address = Query::SServerAddress{ Loopback, m_server.GetPort() };
		source.repository = "/repo";
		ASSERT_TRUE(m_downloads.Initialize(m_dir, std::move(source), icons, [this]()
		{
			{
				std::lock_guard const lock{ m_mutex };

				m_hasResults = true;
			}

			m_wake.notify_one();
		}));
	}

	// Takes results until nothing runs; true when the downloaded games changed meanwhile.
	bool RunUntilIdle()
	{
		auto const deadline{ std::chrono::steady_clock::now() + Patience };
		bool hasChanged{ m_downloads.Update() };

		while (m_downloads.GetPhase() != EDownloadPhase::Idle && std::chrono::steady_clock::now() < deadline)
		{
			{
				std::unique_lock lock{ m_mutex };

				m_wake.wait_until(lock, deadline, [this]() { return m_hasResults; });
				m_hasResults = false;
			}

			hasChanged = m_downloads.Update() || hasChanged;
		}

		EXPECT_EQ(m_downloads.GetPhase(), EDownloadPhase::Idle);

		return hasChanged;
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
// The second reading is answered after the icon, so the icon has been taken by then.
TEST_F(CGameDownloadsTest, IconOfAnOfferedGameIsFetched)
{
	Start(EIndexIcons::Fetch);
	ReadIndex();
	ReadIndex();

	ASSERT_EQ(m_downloads.GetOffers().size(), 1u);
	EXPECT_EQ(m_downloads.GetIcon(m_downloads.GetOffers().front().iconSha256), m_files.at("games/kingpin/icon.png"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IconThatDiffersFromTheIndexIsNotKept)
{
	JsonValue const index = MakeIndex();

	m_files["games/kingpin/icon.png"][0] ^= 1;
	Serve(index);
	Start(EIndexIcons::Fetch);
	ReadIndex();
	ReadIndex();

	ASSERT_EQ(m_downloads.GetOffers().size(), 1u);
	EXPECT_TRUE(m_downloads.GetIcon(m_downloads.GetOffers().front().iconSha256).empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IndexReadWithoutIconsFetchesNoIcon)
{
	Start();
	ReadIndex();
	ReadIndex();

	EXPECT_EQ(m_server.GetNumRequests(), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameDownloadsTest, IconHeldIsNotFetchedAgain)
{
	Start(EIndexIcons::Fetch);
	ReadIndex();
	ReadIndex();
	ReadIndex();

	EXPECT_EQ(m_server.GetNumRequests(), 4u);
}

//////////////////////////////////////////////////////////////////////////
// No Update runs between the reading and the download, so the icon is still pending when the download asks for it.
TEST_F(CGameDownloadsTest, DownloadAskingForAPendingIconInstallsTheGame)
{
	Start(EIndexIcons::Fetch);
	ReadIndex();

	EXPECT_TRUE(Download("kingpin"));
	EXPECT_EQ(GetState("kingpin"), EOfferState::Installed);
	EXPECT_EQ(m_downloads.GetIcon(m_downloads.GetOffers().front().iconSha256), m_files.at("games/kingpin/icon.png"));
}
} // namespace
} // namespace Lkt::Download
