#include "download/lookout_release.hpp"
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace Lkt::Download
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(LookoutRelease, VersionIsThreeNumbers)
{
	EXPECT_EQ(ParseLookoutVersion("1.3.0"), (std::array<uint32_t, 3>{ 1, 3, 0 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(LookoutRelease, VersionsCompareByNumberNotByText)
{
	EXPECT_GT(ParseLookoutVersion("1.10.0"), ParseLookoutVersion("1.9.0"));
}

//////////////////////////////////////////////////////////////////////////
TEST(LookoutRelease, TextThatIsNoVersionIsRefused)
{
	for (std::string_view const text : { "1.4", "v1.4.0", "1.4.0.1", "1..0", "-1.0.0", " 1.4.0", "1.4.0-rc1", "1.4.", "" })
	{
		EXPECT_EQ(ParseLookoutVersion(text), std::nullopt) << text;
	}
}
//////////////////////////////////////////////////////////////////////////
TEST(LookoutRelease, LaterReleaseIsNewer)
{
	EXPECT_TRUE(IsNewerLookout("1.4.0", "1.3.9"));
}

//////////////////////////////////////////////////////////////////////////
TEST(LookoutRelease, SameReleaseIsNotNewer)
{
	EXPECT_FALSE(IsNewerLookout("1.3.0", "1.3.0"));
}

//////////////////////////////////////////////////////////////////////////
TEST(LookoutRelease, NoVersionIsNotNewer)
{
	EXPECT_FALSE(IsNewerLookout("", "1.3.0"));
}
} // namespace
} // namespace Lkt::Download
