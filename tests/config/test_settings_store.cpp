#include "channels.hpp"
#include "fixtures.hpp"
#include "settings_json.hpp"
#include "config/default_settings.hpp"
#include "config/settings.hpp"
#include "config/settings_store.hpp"
#include "query/server_address.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <vector>

namespace Lkt::Config
{
namespace
{
using Fixtures::ConfigChannel;
using Tge::Testing::CExpectedLog;

//////////////////////////////////////////////////////////////////////////
SSettings MakeChangedSettings()
{
	SSettings settings{ MakeDefaultSettings() };

	settings.window.width = 1500;
	settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].favourites = { Query::SServerAddress{ 0xCB007107, 31510 } };

	return settings;
}

//////////////////////////////////////////////////////////////////////////
std::string ReadText(std::filesystem::path const& path)
{
	std::ifstream file{ path, std::ios::binary };

	return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
}

//////////////////////////////////////////////////////////////////////////
void WriteText(std::filesystem::path const& path, std::string_view text)
{
	std::ofstream file{ path, std::ios::binary };

	file << text;
}

//////////////////////////////////////////////////////////////////////////
ino_t GetInode(std::filesystem::path const& path)
{
	struct stat status{};

	EXPECT_EQ(::stat(path.c_str(), &status), 0) << path;

	return status.st_ino;
}

//////////////////////////////////////////////////////////////////////////
class CSettingsStoreTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::error_code error{};
		std::string pattern{ (std::filesystem::temp_directory_path(error) / "lookout-settings-XXXXXX").string() };

		ASSERT_NE(::mkdtemp(pattern.data()), nullptr);
		m_dir = pattern;
		m_file = m_dir / "config.json";
	}

	void TearDown() override
	{
		std::error_code error{};

		std::filesystem::remove_all(m_dir, error);
	}
	// ~testing::Test

	std::filesystem::path m_dir;
	std::filesystem::path m_file;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, MissingFileGivesDefaultsSilently)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };
	CSettingsStore store{};

	store.Initialize(m_dir.string());

	EXPECT_EQ(store.Load(), MakeDefaultSettings());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, SavedSettingsLoadBack)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };
	CSettingsStore writer{};
	CSettingsStore reader{};

	writer.Initialize(m_dir.string());
	writer.Load();
	writer.Save(MakeChangedSettings());
	reader.Initialize(m_dir.string());

	EXPECT_EQ(reader.Load(), MakeChangedSettings());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, SectionOfAGameTheCatalogLacksSurvivesASave)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };
	CSettingsStore store{};

	WriteText(m_file, R"({ "games": { "doom": { "favourites": [ "198.51.100.9:27960" ] } } })");
	store.Initialize(m_dir.string());
	store.Load();
	store.Save(MakeChangedSettings());

	EXPECT_TRUE(ReadText(m_file).contains("198.51.100.9:27960"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, SaveLeavesOnlyTheSettingsFile)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };
	CSettingsStore store{};

	store.Initialize(m_dir.string());
	store.Load();
	store.Save(MakeChangedSettings());

	std::vector<std::filesystem::path> files{};

	for (std::filesystem::directory_entry const& entry : std::filesystem::directory_iterator{ m_dir })
	{
		files.emplace_back(entry.path());
	}

	EXPECT_EQ(files, std::vector<std::filesystem::path>{ m_file });
}

//////////////////////////////////////////////////////////////////////////
// A save renames a new file into place, so an unchanged inode means nothing was written.
TEST_F(CSettingsStoreTest, UnchangedSettingsAreNotRewritten)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };
	CSettingsStore writer{};

	writer.Initialize(m_dir.string());
	writer.Load();
	writer.Save(MakeChangedSettings());

	ino_t const before{ GetInode(m_file) };
	CSettingsStore store{};

	store.Initialize(m_dir.string());
	store.Save(store.Load());

	EXPECT_EQ(GetInode(m_file), before);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, InvalidJsonLogsOneWarning)
{
	CExpectedLog const expected{ ConfigChannel, 1, 0 };
	CSettingsStore store{};

	WriteText(m_file, "{ \"game\": ");
	store.Initialize(m_dir.string());

	EXPECT_EQ(store.Load(), MakeDefaultSettings());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, InvalidJsonIsMovedAside)
{
	CExpectedLog const expected{ ConfigChannel, 1, 0 };
	CSettingsStore store{};

	WriteText(m_file, "{ \"game\": ");
	store.Initialize(m_dir.string());
	store.Load();

	EXPECT_FALSE(std::filesystem::exists(m_file));
	EXPECT_EQ(ReadText(m_dir / "config.json.bad"), "{ \"game\": ");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, InvalidValuesLogOneWarningBetweenThem)
{
	CExpectedLog const expected{ ConfigChannel, 1, 0 };
	CSettingsStore store{};

	WriteText(m_file, R"({ "window": { "width": "wide", "height": true }, "game": "doom" })");
	store.Initialize(m_dir.string());

	EXPECT_EQ(store.Load(), MakeDefaultSettings());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, InvalidValuesKeepTheOriginal)
{
	CExpectedLog const expected{ ConfigChannel, 1, 0 };
	CSettingsStore store{};
	std::string_view const original{ R"({ "window": { "width": "wide" } })" };

	WriteText(m_file, original);
	store.Initialize(m_dir.string());
	store.Save(store.Load());

	EXPECT_EQ(ReadText(m_dir / "config.json.bad"), original);
}

//////////////////////////////////////////////////////////////////////////
// One Warning only: a second load finds nothing left to replace.
TEST_F(CSettingsStoreTest, InvalidValuesAreRepairedOnSave)
{
	CExpectedLog const expected{ ConfigChannel, 1, 0 };
	CSettingsStore store{};
	CSettingsStore reader{};

	WriteText(m_file, R"({ "window": { "width": "wide" } })");
	store.Initialize(m_dir.string());
	store.Save(store.Load());
	reader.Initialize(m_dir.string());
	reader.Load();
}

//////////////////////////////////////////////////////////////////////////
// Saving would have failed loudly on the directory, so no Error proves the save was refused.
TEST_F(CSettingsStoreTest, UnreadableFileIsNeverOverwritten)
{
	CExpectedLog const expected{ ConfigChannel, 1, 0 };
	CSettingsStore store{};

	std::filesystem::create_directory(m_file);
	store.Initialize(m_dir.string());
	store.Load();
	store.Save(MakeChangedSettings());

	EXPECT_TRUE(std::filesystem::is_directory(m_file));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, OversizedFileIsNeverOverwritten)
{
	CExpectedLog const expected{ ConfigChannel, 1, 0 };
	CSettingsStore store{};
	std::string const oversized(1024 * 1024 + 1, ' ');

	WriteText(m_file, oversized);
	store.Initialize(m_dir.string());
	store.Load();
	store.Save(MakeChangedSettings());

	EXPECT_EQ(ReadText(m_file).size(), oversized.size());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, NewerFileIsNeverOverwritten)
{
	CExpectedLog const expected{ ConfigChannel, 1, 0 };
	CSettingsStore store{};
	std::string const newer{ std::format(R"({{ "version": {}, "window": {{ "width": 1700 }} }})", SettingsVersion + 1) };

	WriteText(m_file, newer);
	store.Initialize(m_dir.string());
	store.Load();
	store.Save(MakeChangedSettings());

	EXPECT_EQ(ReadText(m_file), newer);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, SavingKeepsASymlinkedFileLinked)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };
	std::filesystem::path const target{ m_dir / "dotfiles.json" };
	CSettingsStore store{};

	WriteText(target, "{}");
	std::filesystem::create_symlink(target, m_file);
	store.Initialize(m_dir.string());
	store.Load();
	store.Save(MakeChangedSettings());

	EXPECT_TRUE(std::filesystem::is_symlink(m_file));
	EXPECT_NE(ReadText(target), "{}");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsStoreTest, SavingKeepsThePermissions)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };
	CSettingsStore store{};
	struct stat status{};

	WriteText(m_file, "{}");
	std::filesystem::permissions(m_file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
	store.Initialize(m_dir.string());
	store.Load();
	store.Save(MakeChangedSettings());

	ASSERT_EQ(::stat(m_file.c_str(), &status), 0);
	EXPECT_EQ(status.st_mode & 0777, 0600u);
}

//////////////////////////////////////////////////////////////////////////
// A regular file where the directory should be fails even for root, which permissions would not.
TEST_F(CSettingsStoreTest, FailedSaveLogsOneError)
{
	CExpectedLog const expected{ ConfigChannel, 0, 1 };
	std::filesystem::path const configDir{ m_dir / "lookout" };
	CSettingsStore store{};

	std::filesystem::create_directory(configDir);
	store.Initialize(configDir.string());
	store.Load();
	std::filesystem::remove(configDir);
	WriteText(configDir, "");
	store.Save(MakeChangedSettings());
}
} // namespace
} // namespace Lkt::Config
