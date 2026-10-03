#include "config/install_ids.hpp"
#include <gtest/gtest.h>
#include <vector>

namespace Lkt::Config
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(InstallIds, FirstInstallIsOne)
{
	EXPECT_EQ(NextInstallId({}), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST(InstallIds, NextFollowsTheHighest)
{
	std::vector<SGameInstall> const installs{ SGameInstall{ 3, {}, EInstallKind::Command, "a" }, SGameInstall{ 1, {}, EInstallKind::Command, "b" } };

	EXPECT_EQ(NextInstallId(installs), 4u);
}

//////////////////////////////////////////////////////////////////////////
TEST(InstallIds, NextAfterTheLastIdTakesTheSmallestFree)
{
	std::vector<SGameInstall> const installs{
		SGameInstall{ MaxInstallId, {}, EInstallKind::Command, "a" },
		SGameInstall{ 1, {}, EInstallKind::Command, "b" } };

	EXPECT_EQ(NextInstallId(installs), 2u);
}
} // namespace
} // namespace Lkt::Config
