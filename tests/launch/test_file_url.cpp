#include "styled_file_url.hpp"
#include <gtest/gtest.h>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, PlainPathIsKept)
{
	EXPECT_EQ(ToFileUrl("/home/joe/.local/state/lookout/logs", EPathStyle::Posix), "file:///home/joe/.local/state/lookout/logs");
}

//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, ReservedCharactersAreEncoded)
{
	EXPECT_EQ(ToFileUrl("/home/a b/100%/#1?", EPathStyle::Posix), "file:///home/a%20b/100%25/%231%3F");
}

//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, Utf8IsEncodedByteByByte)
{
	EXPECT_EQ(ToFileUrl("/home/jörg", EPathStyle::Posix), "file:///home/j%C3%B6rg");
}

//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, BackslashInALinuxNameIsEncoded)
{
	EXPECT_EQ(ToFileUrl("/home/joe/a\\b", EPathStyle::Posix), "file:///home/joe/a%5Cb");
}

//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, WindowsDrivePathGetsThreeSlashes)
{
	EXPECT_EQ(ToFileUrl("C:\\Users\\joe\\AppData\\Local\\Lookout\\logs", EPathStyle::Windows), "file:///C:/Users/joe/AppData/Local/Lookout/logs");
}

//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, WindowsReservedCharactersAreEncoded)
{
	EXPECT_EQ(ToFileUrl("C:\\My Games\\100%\\#1", EPathStyle::Windows), "file:///C:/My%20Games/100%25/%231");
}

//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, WindowsKeepsUtf8)
{
	EXPECT_EQ(ToFileUrl("C:\\Users\\jörg", EPathStyle::Windows), "file:///C:/Users/jörg");
}

//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, WindowsShareNamesItsServer)
{
	EXPECT_EQ(ToFileUrl("\\\\server\\games\\kingpin", EPathStyle::Windows), "file://server/games/kingpin");
}

//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, WindowsLongPathPrefixesGo)
{
	EXPECT_EQ(ToFileUrl("\\\\?\\C:\\Games", EPathStyle::Windows), "file:///C:/Games");
	EXPECT_EQ(ToFileUrl("\\\\?\\UNC\\server\\games", EPathStyle::Windows), "file://server/games");
}
} // namespace
} // namespace Lkt::Launch
