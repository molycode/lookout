#include "launch/home_path.hpp"
#include <gtest/gtest.h>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(HomePath, PathUnderHomeStartsWithTilde)
{
	EXPECT_EQ(ShortenHome("/home/joe/Games/Kingpin", "/home/joe"), "~/Games/Kingpin");
}

//////////////////////////////////////////////////////////////////////////
TEST(HomePath, HomeItselfIsTilde)
{
	EXPECT_EQ(ShortenHome("/home/joe", "/home/joe"), "~");
}

//////////////////////////////////////////////////////////////////////////
TEST(HomePath, SiblingWithTheSamePrefixIsKept)
{
	EXPECT_EQ(ShortenHome("/home/joey/Games", "/home/joe"), "/home/joey/Games");
}

//////////////////////////////////////////////////////////////////////////
TEST(HomePath, UnknownHomeKeepsThePath)
{
	EXPECT_EQ(ShortenHome("/home/joe/Games", {}), "/home/joe/Games");
}
} // namespace
} // namespace Lkt::Launch
