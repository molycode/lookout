#include "connect_args.hpp"
#include "fixtures.hpp"
#include "launch/describe_password_rules.hpp"
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
// The built-in games share these rules; Kingpin's stand for them.
Query::SJoinCommand const& Join()
{
	return Fixtures::GetGameByKey("kingpin").join;
}

//////////////////////////////////////////////////////////////////////////
bool IsRefused(std::string password)
{
	return BuildConnectArgs(Join(), SConnectRequest{ Server, std::move(password) }).error_or(ELaunchError::SpawnFailed) == ELaunchError::UnsupportedPassword;
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, WithoutPasswordOnlyConnects)
{
	EXPECT_EQ(BuildConnectArgs(Join(), SConnectRequest{ Server, {} }), (Arguments{ "+connect", "203.0.113.7:31510" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, PasswordIsSetBeforeConnecting)
{
	EXPECT_EQ(BuildConnectArgs(Join(), SConnectRequest{ Server, "s3cret" }), (Arguments{ "+set", "password", "s3cret", "+connect", "203.0.113.7:31510" }));
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

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, PasswordUpToTheLimitIsCarried)
{
	EXPECT_FALSE(IsRefused(std::string(63, 'x')));
	EXPECT_TRUE(IsRefused(std::string(64, 'x')));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, PlaceholdersAreFilledInsideAnArgument)
{
	Query::SJoinCommand join{};

	join.arguments = { "{address}" };
	join.passwordArguments = { "{address}?password={password}" };
	join.password.maxLength = 63;

	EXPECT_EQ(BuildConnectArgs(join, SConnectRequest{ Server, "s3cret" }), (Arguments{ "203.0.113.7:31510?password=s3cret" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, PasswordThatSpellsAPlaceholderStaysAsTyped)
{
	EXPECT_EQ(BuildConnectArgs(Join(), SConnectRequest{ Server, "{address}" }), (Arguments{ "+set", "password", "{address}", "+connect", "203.0.113.7:31510" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ConnectArgs, RulesAreDescribed)
{
	EXPECT_EQ(DescribePasswordRules(Join().password), "up to 63 printable ASCII characters, without spaces, \", $, ;, +, \\, // or /*");
}
} // namespace
} // namespace Lkt::Launch
