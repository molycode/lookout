#include "channels.hpp"
#include "registry.hpp"
#include <tge/platform.hpp>
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <format>
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
// The registry is Windows'; Linux reads no store's entries.
#if defined(TGE_PLATFORM_WINDOWS)
using Fixtures::LaunchChannel;
using Tge::Testing::CExpectedLog;

//////////////////////////////////////////////////////////////////////////
// A key of the test's own under HKCU, which is the same in both registry views, deleted with everything in it.
class CRegistryTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		m_key = std::format("Software\\Lookout tests {}", GetCurrentProcessId());
	}

	void TearDown() override
	{
		RegDeleteTreeW(HKEY_CURRENT_USER, Widen(m_key).c_str());
	}
	// ~testing::Test

	static std::wstring Widen(std::string_view ascii)
	{
		return std::wstring{ ascii.begin(), ascii.end() };
	}

	void WriteValue(std::string_view subkey, std::wstring_view name, std::wstring_view value) const
	{
		HKEY key{ nullptr };

		ASSERT_EQ(RegCreateKeyExW(HKEY_CURRENT_USER, Widen(std::format("{}\\{}", m_key, subkey)).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr), ERROR_SUCCESS);
		EXPECT_EQ(RegSetValueExW(key, std::wstring{ name }.c_str(), 0, REG_SZ, reinterpret_cast<BYTE const*>(value.data()),
			static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))), ERROR_SUCCESS);
		RegCloseKey(key);
	}

	std::string m_key{};
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CRegistryTest, GogGamesAreRead)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteValue("Games\\1207658912", L"path", L"D:\\GOG Games\\Kingpin");
	WriteValue("Games\\1207658912", L"gameName", L"Kingpin: Life of Crime");

	std::vector<SGogInstall> const installs{ ReadGogInstalls(ERegistryRoot::CurrentUser, m_key + "\\Games") };

	ASSERT_EQ(installs.size(), 1u);
	EXPECT_EQ(installs.front().id, 1207658912u);
	EXPECT_EQ(installs.front().folder, std::filesystem::path{ L"D:\\GOG Games\\Kingpin" });
	EXPECT_EQ(installs.front().name, "Kingpin: Life of Crime");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CRegistryTest, GogEntryWithoutAFolderIsSkipped)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteValue("Games\\1207658912", L"gameName", L"Kingpin: Life of Crime");

	EXPECT_TRUE(ReadGogInstalls(ERegistryRoot::CurrentUser, m_key + "\\Games").empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CRegistryTest, MissingGogKeyIsNoGames)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	EXPECT_TRUE(ReadGogInstalls(ERegistryRoot::CurrentUser, m_key + "\\Games").empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CRegistryTest, RegistryFolderIsRead)
{
	WriteValue("Valve\\Steam", L"SteamPath", L"c:/games");

	EXPECT_EQ(ReadRegistryFolder(ERegistryRoot::CurrentUser, m_key + "\\Valve\\Steam", "SteamPath"), std::filesystem::path{ "c:/games" });
}
#endif // defined(TGE_PLATFORM_WINDOWS)
} // namespace
} // namespace Lkt::Launch
