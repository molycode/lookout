#include <tge/platform.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <string>

#if defined(TGE_PLATFORM_WINDOWS)
//////////////////////////////////////////////////////////////////////////
// The suite carries the programs' manifest; without its UTF-8 code page, narrow text converts through the ANSI one.
TEST(Manifest, NarrowPathsAreUtf8)
{
	std::filesystem::path const path{ std::string{ "Spiele \xC3\xB6" } };

	EXPECT_EQ(path.wstring(), std::wstring{ L"Spiele ö" });
}
#endif // defined(TGE_PLATFORM_WINDOWS)
