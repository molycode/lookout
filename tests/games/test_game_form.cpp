#include "merge_patch.hpp"
#include "games/check_game.hpp"
#include "games/game_form.hpp"
#include "json/json.hpp"
#include "query/game_catalog.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

//////////////////////////////////////////////////////////////////////////
std::string ReadGameFile(std::filesystem::path const& path)
{
	std::ifstream file{ path, std::ios::binary };

	return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
}

//////////////////////////////////////////////////////////////////////////
std::string ReadDownloaded(std::string_view key)
{
	return ReadGameFile(std::filesystem::path{ LKT_LOOKOUT_GAMES_DIR } / "games" / key / "game.json");
}

//////////////////////////////////////////////////////////////////////////
SGameFormNode ReadForm(std::string_view text)
{
	std::expected<SGameFormNode, std::string> form{ ReadGameForm(text) };

	EXPECT_TRUE(form.has_value()) << form.error();

	return form.has_value() ? std::move(*form) : SGameFormNode{};
}

//////////////////////////////////////////////////////////////////////////
bool HasComment(JsonValue const& value)
{
	bool hasComment{ false };

	if (value.is_structured())
	{
		for (auto const& item : value.items())
		{
			hasComment = hasComment || (value.is_object() && std::string_view{ item.key() }.starts_with("//")) || HasComment(item.value());
		}
	}

	return hasComment;
}

//////////////////////////////////////////////////////////////////////////
std::string ProblemOf(std::string_view text)
{
	std::expected<SGameFormNode, std::string> const form{ ReadGameForm(text) };

	return form.has_value() ? std::string{} : form.error();
}

//////////////////////////////////////////////////////////////////////////
// Opening a game and saving it untouched must change nothing: ordered JSON compares fields in order, comments included.
TEST(GameForm, EveryDownloadedGameWritesBackAsItWas)
{
	for (std::filesystem::directory_entry const& entry : std::filesystem::directory_iterator{ std::filesystem::path{ LKT_LOOKOUT_GAMES_DIR } / "games" })
	{
		std::string const text{ ReadGameFile(entry.path() / "game.json") };

		EXPECT_EQ(JsonValue::parse(WriteGameForm(ReadForm(text))), JsonValue::parse(text)) << entry.path();
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, OneEditedFieldMakesAOneFieldPatch)
{
	std::string const text{ ReadDownloaded("kingpin") };
	SGameFormNode form{ ReadForm(text) };

	GetFormField(form, "name").text = "Kingpin, mine";

	std::expected<std::optional<std::string>, std::string> const patch{ MakePatch(text, WriteGameForm(form)) };

	ASSERT_TRUE(patch.has_value() && patch->has_value());
	EXPECT_EQ(JsonValue::parse(**patch), JsonValue::parse(R"json({ "name": "Kingpin, mine" })json"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, WholeNumberIsWrittenAsANumber)
{
	SGameFormNode form{ ReadForm(ReadDownloaded("kingpin")) };

	GetFormField(GetFormField(form, "masters").children.front(), "port").text = "27951";

	JsonValue const written = JsonValue::parse(WriteGameForm(form));

	ASSERT_TRUE(written["masters"][0]["port"].is_number_unsigned());
	EXPECT_EQ(written["masters"][0]["port"].get<uint64_t>(), 27951u);
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, NumberThatIsNotOneStaysTextForTheCheck)
{
	SGameFormNode form{ ReadForm(ReadDownloaded("kingpin")) };

	GetFormField(GetFormField(form, "masters").children.front(), "port").text = "abc";

	std::expected<void, SFieldProblem> const checked{ CheckGameText(WriteGameForm(form), Query::GetProtocolCatalog()) };

	ASSERT_FALSE(checked.has_value());
	EXPECT_EQ(checked.error().path, "masters[0].port");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, EscapeCharacterIsShownEscaped)
{
	SGameFormNode const form{ ReadForm(ReadDownloaded("ut2004")) };
	SGameFormNode const* const pCodes{ FindFormField(*FindFormField(form, "text"), "colourCodes") };

	ASSERT_NE(pCodes, nullptr);
	EXPECT_EQ(FindFormField(*pCodes, "escape")->text, "\\u001b");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, BackslashAmongCharactersIsShownDoubled)
{
	SGameFormNode const form{ ReadForm(ReadDownloaded("kingpin")) };
	SGameFormNode const* const pPassword{ FindFormField(*FindFormField(form, "join"), "password") };

	ASSERT_NE(pPassword, nullptr);
	EXPECT_EQ(FindFormField(*pPassword, "refusedCharacters")->text, " \"$;+\\\\");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, EmptiedOptionalFieldLeavesItsNoteOut)
{
	SGameFormNode form{ ReadForm(ReadDownloaded("ut2004")) };

	GetFormField(form, "queryPortOffset").text.clear();

	JsonValue const written = JsonValue::parse(WriteGameForm(form));

	EXPECT_FALSE(written.contains("queryPortOffset"));
	EXPECT_FALSE(written.contains("//queryPortOffset"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, EmptyListIsKept)
{
	JsonValue game = JsonValue::parse(ReadDownloaded("kingpin"));

	game["foreignServers"] = JsonValue::array();

	JsonValue const written = JsonValue::parse(WriteGameForm(ReadForm(game.dump())));

	ASSERT_TRUE(written.contains("foreignServers"));
	EXPECT_TRUE(written["foreignServers"].empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, RgbCodesWriteNoPalette)
{
	SGameFormNode form{ ReadForm(ReadDownloaded("ut2004")) };

	GetFormField(GetFormField(GetFormField(form, "text"), "colourCodes"), "palette").texts.emplace_back("#000000");

	EXPECT_FALSE(JsonValue::parse(WriteGameForm(form))["text"]["colourCodes"].contains("palette"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, ClearedNotesWriteNoComments)
{
	SGameFormNode form{ ReadForm(ReadDownloaded("kingpin")) };

	ClearFormNotes(form);

	EXPECT_FALSE(HasComment(JsonValue::parse(WriteGameForm(form))));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, NewGameAsksForItsNameFirst)
{
	std::expected<void, SFieldProblem> const checked{ CheckGameText(WriteGameForm(MakeGameForm()), Query::GetProtocolCatalog()) };

	ASSERT_FALSE(checked.has_value());
	EXPECT_EQ(checked.error().path, "name");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, NewListItemHasItsFields)
{
	SGameFormNode const item{ MakeGameFormItem("masters") };

	EXPECT_NE(FindFormField(item, "host"), nullptr);
	EXPECT_NE(FindFormField(item, "port"), nullptr);
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, TextThatIsNotJsonCannotBeShown)
{
	EXPECT_TRUE(ProblemOf("{ \"format\": 1,").starts_with("it is not valid JSON: ")) << ProblemOf("{ \"format\": 1,");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, UnknownFieldCannotBeShown)
{
	EXPECT_EQ(ProblemOf(R"json({ "format": 1, "keys": { "hostnme": "hostname" } })json"), "keys.hostnme: is not a field of format 1");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, GroupWhereAListBelongsCannotBeShown)
{
	EXPECT_EQ(ProblemOf(R"json({ "format": 1, "masters": { "host": "master.example" } })json"), "masters: must be an array");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, NoteThatIsNotTextCannotBeShown)
{
	EXPECT_EQ(ProblemOf(R"json({ "format": 1, "//name": "", "name": "Mine" })json"), "//name: must be a non-empty string without NUL");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameForm, NewerFormatCannotBeShown)
{
	EXPECT_EQ(ProblemOf(R"json({ "format": 2, "name": "Mine" })json"), "format: is 2, which needs a newer Lookout");
}
} // namespace
} // namespace Lkt::Games
