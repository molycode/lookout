#include "launch/file_url.hpp"
#include <gtest/gtest.h>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, PlainPathIsKept)
{
	EXPECT_EQ(ToFileUrl("/home/joe/.local/state/lookout/logs"), "file:///home/joe/.local/state/lookout/logs");
}

//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, ReservedCharactersAreEncoded)
{
	EXPECT_EQ(ToFileUrl("/home/a b/100%/#1?"), "file:///home/a%20b/100%25/%231%3F");
}

//////////////////////////////////////////////////////////////////////////
TEST(FileUrl, Utf8IsEncodedByteByByte)
{
	EXPECT_EQ(ToFileUrl("/home/jörg"), "file:///home/j%C3%B6rg");
}
} // namespace
} // namespace Lkt::Launch
