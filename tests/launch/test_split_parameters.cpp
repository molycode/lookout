#include "launch/split_parameters.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <string>
#include <vector>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(SplitParameters, SpacesSeparateParameters)
{
	EXPECT_EQ(SplitParameters("-windowed +set fov 110"), (std::vector<std::string>{ "-windowed", "+set", "fov", "110" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(SplitParameters, QuotesKeepAParameterWhole)
{
	EXPECT_EQ(SplitParameters("+name \"two words\""), (std::vector<std::string>{ "+name", "two words" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(SplitParameters, NoneAreNothing)
{
	EXPECT_EQ(SplitParameters(""), std::vector<std::string>{});
}

//////////////////////////////////////////////////////////////////////////
TEST(SplitParameters, UnclosedQuoteIsRefused)
{
	EXPECT_EQ(SplitParameters("+name \"two words").error_or(ELaunchError::NoLauncher), ELaunchError::BadParameters);
}
} // namespace
} // namespace Lkt::Launch
