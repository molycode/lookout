#include "temp_dir.hpp"
#include "launch/found_stores.hpp"
#include "query/store_definition.hpp"
#include <tge/platform.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(TGE_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <windows.h>
#endif // defined(TGE_PLATFORM_WINDOWS)

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
void WriteProgram(std::filesystem::path const& path)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream{ path, std::ios::binary } << "";
	std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
}

//////////////////////////////////////////////////////////////////////////
class CFoundStoresTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-found-stores"));
		m_root = m_tempDir.GetPath();
		m_environment.home = m_root / "home";
		m_environment.dataHome = m_root / "data";
		m_environment.searchPath = (m_root / "bin").string();
		m_store = Query::SStoreDefinition{ "steam", "Steam", { "-applaunch", "{id}" }, Query::EStoreLibraries::Steam, {}, {} };
#if defined(TGE_PLATFORM_WINDOWS)
		m_environment.readStoreFolder = ReadLaunchEnvironment().readStoreFolder;
#endif // defined(TGE_PLATFORM_WINDOWS)
	}

	void TearDown() override
	{
#if defined(TGE_PLATFORM_WINDOWS)
		RegDeleteTreeW(HKEY_CURRENT_USER, Widen(m_key).c_str());
#endif // defined(TGE_PLATFORM_WINDOWS)
		m_tempDir.Terminate();
	}
	// ~testing::Test

	std::vector<SFoundStore> Find() const
	{
		return FindStores(std::span<Query::SStoreDefinition const>{ &m_store, 1 }, m_environment, m_located);
	}

#if defined(TGE_PLATFORM_WINDOWS)
	static std::wstring Widen(std::string_view ascii)
	{
		return std::wstring{ ascii.begin(), ascii.end() };
	}

	void WriteFolderValue(std::filesystem::path const& folder) const
	{
		HKEY key{ nullptr };
		std::wstring const value{ folder.wstring() };

		ASSERT_EQ(RegCreateKeyExW(HKEY_CURRENT_USER, Widen(m_key).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr), ERROR_SUCCESS);
		EXPECT_EQ(RegSetValueExW(key, L"SteamPath", 0, REG_SZ, reinterpret_cast<BYTE const*>(value.c_str()),
			static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))), ERROR_SUCCESS);
		RegCloseKey(key);
	}

	std::string m_key{ std::format("Software\\Lookout store tests {}", GetCurrentProcessId()) };
#endif // defined(TGE_PLATFORM_WINDOWS)

	Lkt::Fixtures::CTempDir m_tempDir{};
	std::filesystem::path m_root{};
	SLaunchEnvironment m_environment{};
	Query::SStoreDefinition m_store{};
	std::vector<SLocatedProgram> m_located{};
};

#if defined(TGE_PLATFORM_LINUX)
//////////////////////////////////////////////////////////////////////////
TEST_F(CFoundStoresTest, FolderWithItsProgramIsFound)
{
	std::filesystem::create_directories(m_environment.dataHome / "Steam");
	WriteProgram(m_root / "bin" / "steam");
	m_store.onLinux = { Query::SLinuxStoreInstall{ "{dataHome}/Steam", { "steam" } } };

	std::vector<SFoundStore> const stores{ Find() };

	ASSERT_EQ(stores.size(), 1u);
	EXPECT_EQ(stores.front().program, m_root / "bin" / "steam");
	EXPECT_EQ(stores.front().libraryFolders, std::vector<std::filesystem::path>{ m_environment.dataHome / "Steam" });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFoundStoresTest, ProgramWithoutItsFolderIsNotFound)
{
	WriteProgram(m_root / "bin" / "steam");
	m_store.onLinux = { Query::SLinuxStoreInstall{ "{dataHome}/Steam", { "steam" } } };

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
// So a game its libraries hold can ask where the program is.
TEST_F(CFoundStoresTest, FolderWithoutItsProgramIsFoundWithoutOne)
{
	std::filesystem::create_directories(m_environment.dataHome / "Steam");
	m_store.onLinux = { Query::SLinuxStoreInstall{ "{dataHome}/Steam", { "steam" } } };

	std::vector<SFoundStore> const stores{ Find() };

	ASSERT_EQ(stores.size(), 1u);
	EXPECT_TRUE(stores.front().program.empty());
	EXPECT_EQ(stores.front().libraryFolders, std::vector<std::filesystem::path>{ m_environment.dataHome / "Steam" });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFoundStoresTest, LocatedProgramWinsOverTheFilesPrograms)
{
	std::filesystem::create_directories(m_environment.dataHome / "Steam");
	WriteProgram(m_root / "bin" / "steam");
	WriteProgram(m_root / "elsewhere" / "steam.sh");
	m_store.onLinux = { Query::SLinuxStoreInstall{ "{dataHome}/Steam", { "steam" } } };
	m_located = { SLocatedProgram{ "steam", m_root / "elsewhere" / "steam.sh" } };

	std::vector<SFoundStore> const stores{ Find() };

	ASSERT_EQ(stores.size(), 1u);
	EXPECT_EQ(stores.front().program, m_root / "elsewhere" / "steam.sh");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFoundStoresTest, LocatedProgramThatIsGoneLeavesTheStoreWithoutOne)
{
	std::filesystem::create_directories(m_environment.dataHome / "Steam");
	WriteProgram(m_root / "bin" / "steam");
	m_store.onLinux = { Query::SLinuxStoreInstall{ "{dataHome}/Steam", { "steam" } } };
	m_located = { SLocatedProgram{ "steam", m_root / "gone" / "steam.sh" } };

	std::vector<SFoundStore> const stores{ Find() };

	ASSERT_EQ(stores.size(), 1u);
	EXPECT_TRUE(stores.front().program.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFoundStoresTest, FirstProgramThatRunsIsTaken)
{
	std::filesystem::create_directories(m_environment.home / "snap");
	WriteProgram(m_environment.home / "bin" / "steam");
	m_store.onLinux = { Query::SLinuxStoreInstall{ "{home}/snap", { "{home}/missing/steam", "{home}/bin/steam" } } };

	std::vector<SFoundStore> const stores{ Find() };

	ASSERT_EQ(stores.size(), 1u);
	EXPECT_EQ(stores.front().program, m_environment.home / "bin" / "steam");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFoundStoresTest, PlaceholderTheEnvironmentCannotFillFindsNothing)
{
	std::filesystem::create_directories(m_environment.home / "Steam");
	WriteProgram(m_root / "bin" / "steam");
	m_environment.home.clear();
	m_store.onLinux = { Query::SLinuxStoreInstall{ "{home}/Steam", { "steam" } } };

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFoundStoresTest, SameInstallListedTwiceIsFoundOnce)
{
	std::filesystem::create_directories(m_environment.dataHome / "Steam");
	WriteProgram(m_root / "bin" / "steam");
	m_store.onLinux = { Query::SLinuxStoreInstall{ "{dataHome}/Steam", { "steam" } }, Query::SLinuxStoreInstall{ "{dataHome}/Steam", { "steam" } } };

	EXPECT_EQ(Find().size(), 1u);
}
#elif defined(TGE_PLATFORM_WINDOWS)
//////////////////////////////////////////////////////////////////////////
TEST_F(CFoundStoresTest, RegistryFolderWithItsProgramIsFound)
{
	std::filesystem::path const folder{ m_root / "Steam" };

	WriteProgram(folder / "steam.exe");
	WriteFolderValue(folder);
	m_store.onWindows = { Query::SWindowsStoreInstall{ Query::SStoreRegistryValue{ Query::EStoreRegistryRoot::CurrentUser, m_key, "SteamPath" }, "steam.exe" } };

	std::vector<SFoundStore> const stores{ Find() };

	ASSERT_EQ(stores.size(), 1u);
	EXPECT_EQ(stores.front().program, folder / "steam.exe");
	EXPECT_EQ(stores.front().libraryFolders, std::vector<std::filesystem::path>{ folder });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFoundStoresTest, MissingRegistryValueFindsNothing)
{
	m_store.onWindows = { Query::SWindowsStoreInstall{ Query::SStoreRegistryValue{ Query::EStoreRegistryRoot::CurrentUser, m_key, "SteamPath" }, "steam.exe" } };

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFoundStoresTest, EnvironmentWithoutAReaderLeavesTheRegistryAlone)
{
	std::filesystem::path const folder{ m_root / "Steam" };

	WriteProgram(folder / "steam.exe");
	WriteFolderValue(folder);
	m_store.onWindows = { Query::SWindowsStoreInstall{ Query::SStoreRegistryValue{ Query::EStoreRegistryRoot::CurrentUser, m_key, "SteamPath" }, "steam.exe" } };
	m_environment.readStoreFolder = nullptr;

	EXPECT_TRUE(Find().empty());
}
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_LINUX)
} // namespace
} // namespace Lkt::Launch
