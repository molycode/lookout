#include "master_warnings.hpp"
#include <gtest/gtest.h>

namespace Lkt::Net
{
namespace
{
constexpr SMasterId Master{ Query::EGame{ 0 }, 0 };
constexpr SMasterId OtherMaster{ Query::EGame{ 0 }, 1 };

//////////////////////////////////////////////////////////////////////////
TEST(MasterWarnings, FirstFailureIsWarned)
{
	CMasterWarnings warnings{};

	EXPECT_TRUE(warnings.RecordFailure(Master, "did not answer"));
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterWarnings, SameFailureAgainIsNotWarned)
{
	CMasterWarnings warnings{};

	warnings.RecordFailure(Master, "did not answer");

	EXPECT_FALSE(warnings.RecordFailure(Master, "did not answer"));
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterWarnings, OtherFailureIsWarned)
{
	CMasterWarnings warnings{};

	warnings.RecordFailure(Master, "did not answer");

	EXPECT_TRUE(warnings.RecordFailure(Master, "refused the query"));
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterWarnings, FailureAfterAnAnswerIsWarnedAgain)
{
	CMasterWarnings warnings{};

	warnings.RecordFailure(Master, "did not answer");
	warnings.RecordAnswer(Master);

	EXPECT_TRUE(warnings.RecordFailure(Master, "did not answer"));
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterWarnings, EachMasterIsRememberedApart)
{
	CMasterWarnings warnings{};

	warnings.RecordFailure(Master, "did not answer");

	EXPECT_TRUE(warnings.RecordFailure(OtherMaster, "did not answer"));
}
} // namespace
} // namespace Lkt::Net
