#include "key_values.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
std::vector<SKeyValue> Parse(std::string_view text)
{
	std::expected<std::vector<SKeyValue>, EKeyValuesError> const parsed{ ParseKeyValues(text) };

	EXPECT_TRUE(parsed.has_value()) << ToString(parsed.error());

	return parsed.value_or(std::vector<SKeyValue>{});
}

//////////////////////////////////////////////////////////////////////////
std::string ValueAt(std::vector<SKeyValue> const& values, std::string_view path)
{
	auto const it{ std::ranges::find(values, path, &SKeyValue::path) };

	return (it != values.end()) ? it->value : std::string{ "(absent)" };
}

//////////////////////////////////////////////////////////////////////////
EKeyValuesError ParseError(std::string_view text)
{
	std::expected<std::vector<SKeyValue>, EKeyValuesError> const parsed{ ParseKeyValues(text) };

	EXPECT_FALSE(parsed.has_value());

	return parsed.has_value() ? EKeyValuesError::UnexpectedCharacter : parsed.error();
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, ReadsNestedGroups)
{
	std::vector<SKeyValue> const values{ Parse("\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Games\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"38430\"\t\t\"739403242\"\n\t\t}\n\t}\n}\n") };

	ASSERT_EQ(values.size(), 2u);
	EXPECT_EQ(ValueAt(values, "libraryfolders/0/path"), "C:\\Games");
	EXPECT_EQ(ValueAt(values, "libraryfolders/0/apps/38430"), "739403242");
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, KeysAreLowerCased)
{
	EXPECT_EQ(ValueAt(Parse(R"("AppState" { "StateFlags" "4" })"), "appstate/stateflags"), "4");
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, EscapedBackslashesAndQuotesAreDecoded)
{
	EXPECT_EQ(ValueAt(Parse(R"("name" "say \"hi\" from D:\\Lib")"), "name"), "say \"hi\" from D:\\Lib");
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, OtherBackslashesAreKept)
{
	EXPECT_EQ(ValueAt(Parse(R"("path" "D:\SteamLibrary\new")"), "path"), "D:\\SteamLibrary\\new");
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, CommentsAndCarriageReturnsAreBlanks)
{
	EXPECT_EQ(ValueAt(Parse("// written by Steam\r\n\"group\"\r\n{\r\n\t\"key\" \"value\" // trailing\r\n}\r\n"), "group/key"), "value");
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, UnterminatedStringFails)
{
	EXPECT_EQ(ParseError(R"("group" { "key" "value)"), EKeyValuesError::UnterminatedString);
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, UnclosedGroupFails)
{
	EXPECT_EQ(ParseError(R"("group" { "key" "value")"), EKeyValuesError::UnclosedGroup);
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, StrayCloseFails)
{
	EXPECT_EQ(ParseError(R"("key" "value" })"), EKeyValuesError::UnexpectedClose);
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, KeyWithoutValueFails)
{
	EXPECT_EQ(ParseError(R"("group" { "key" })"), EKeyValuesError::MissingValue);
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, UnquotedTokenFails)
{
	EXPECT_EQ(ParseError("key \"value\""), EKeyValuesError::UnexpectedCharacter);
}

//////////////////////////////////////////////////////////////////////////
TEST(KeyValues, DeepNestingFails)
{
	std::string text{};

	for (int depth{ 0 }; depth < 40; ++depth)
	{
		text += "\"g\" { ";
	}

	EXPECT_EQ(ParseError(text), EKeyValuesError::TooDeep);
}
} // namespace
} // namespace Lkt::Launch
