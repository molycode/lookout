#include "read_release.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Download
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(ReadRelease, TagNamesTheVersion)
{
	std::expected<std::string, std::string> const version{ ReadReleaseVersion(R"({ "tag_name": "v1.5.0", "name": "Lookout 1.5.0", "draft": false })") };

	ASSERT_TRUE(version.has_value()) << version.error();
	EXPECT_EQ(*version, "1.5.0");
}

//////////////////////////////////////////////////////////////////////////
TEST(ReadRelease, TagWithoutItsVIsReadToo)
{
	EXPECT_EQ(ReadReleaseVersion(R"({ "tag_name": "1.5.0" })").value_or(""), "1.5.0");
}

//////////////////////////////////////////////////////////////////////////
TEST(ReadRelease, ReleaseWithoutTagIsRefused)
{
	for (std::string_view const text : { R"({})", R"({ "tag_name": 150 })", R"([ "v1.5.0" ])" })
	{
		EXPECT_FALSE(ReadReleaseVersion(text).has_value()) << text;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(ReadRelease, TagThatIsNoVersionIsRefused)
{
	for (std::string_view const text : { R"({ "tag_name": "v1.5" })", R"({ "tag_name": "vv1.5.0" })", R"({ "tag_name": "latest" })" })
	{
		EXPECT_FALSE(ReadReleaseVersion(text).has_value()) << text;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(ReadRelease, ReplyThatIsNoJsonIsRefused)
{
	EXPECT_FALSE(ReadReleaseVersion("<html>rate limited</html>").has_value());
}
} // namespace
} // namespace Lkt::Download
