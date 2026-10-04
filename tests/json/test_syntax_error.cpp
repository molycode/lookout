#include "json/syntax_error.hpp"
#include <gtest/gtest.h>
#include <string>

namespace Lkt::Json
{
namespace
{
constexpr bool IgnoreComments{ true };
constexpr bool RejectComments{ false };

//////////////////////////////////////////////////////////////////////////
TEST(SyntaxError, NamesTheLineItIsOn)
{
	std::string const error{ DescribeSyntaxError("{\n\t\"a\": 1\n\t\"b\": 2\n}", RejectComments) };

	EXPECT_TRUE(error.starts_with("line 3, column ")) << error;
}

//////////////////////////////////////////////////////////////////////////
TEST(SyntaxError, IgnoredCommentIsNotTheError)
{
	std::string const error{ DescribeSyntaxError("// a note\n{ \"a\": }", IgnoreComments) };

	EXPECT_TRUE(error.starts_with("line 2, column ")) << error;
}

//////////////////////////////////////////////////////////////////////////
TEST(SyntaxError, ValidJsonHasNone)
{
	EXPECT_EQ(DescribeSyntaxError("{ \"a\": [ 1, 2 ] }", RejectComments), "");
}
} // namespace
} // namespace Lkt::Json
