#include "connect_args.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace Lkt::Launch
{
namespace
{
using Arguments = std::vector<std::string>;

// 203.0.113.7:31510.
constexpr Query::SServerAddress Server{ 0xCB007107, 31510 };

//////////////////////////////////////////////////////////////////////////
bool IsRefused(std::string password)
{
	return BuildConnectArgs(SConnectRequest{ Server, std::move(password) }).error_or(ELaunchError::SpawnFailed) == ELaunchError::UnsupportedPassword;
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, WithoutPasswordOnlyConnects)
{
	EXPECT_EQ(BuildConnectArgs(SConnectRequest{ Server, {} }), (Arguments{ "+connect", "203.0.113.7:31510" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, PasswordIsSetBeforeConnecting)
{
	EXPECT_EQ(BuildConnectArgs(SConnectRequest{ Server, "s3cret" }), (Arguments{ "+set", "password", "s3cret", "+connect", "203.0.113.7:31510" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, SpaceIsRefused)
{
	EXPECT_TRUE(IsRefused("open sesame"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, QuoteIsRefused)
{
	EXPECT_TRUE(IsRefused("open\"sesame"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, DollarIsRefused)
{
	EXPECT_TRUE(IsRefused("open$sesame"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, SemicolonIsRefused)
{
	EXPECT_TRUE(IsRefused("open;sesame"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, PlusIsRefused)
{
	EXPECT_TRUE(IsRefused("open+sesame"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, BackslashIsRefused)
{
	EXPECT_TRUE(IsRefused("open\\sesame"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, LineCommentIsRefused)
{
	EXPECT_TRUE(IsRefused("open//sesame"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, BlockCommentIsRefused)
{
	EXPECT_TRUE(IsRefused("open/*sesame"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, ControlCharacterIsRefused)
{
	EXPECT_TRUE(IsRefused("open\tsesame"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, NonAsciiIsRefused)
{
	EXPECT_TRUE(IsRefused("s\xC3\xA9same"));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, SixtyFourCharactersAreRefused)
{
	EXPECT_TRUE(IsRefused(std::string(64, 'k')));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, SixtyThreeCharactersAreAccepted)
{
	EXPECT_FALSE(IsRefused(std::string(63, 'k')));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, OtherPunctuationIsAccepted)
{
	EXPECT_FALSE(IsRefused("it's%~#/!"));
}
} // namespace
} // namespace Lkt::Launch
