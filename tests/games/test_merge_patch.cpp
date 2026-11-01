#include "merge_patch.hpp"
#include "json/json.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr std::string_view FormatOneDownload{ R"json({ "format": 1, "name": "Kingpin",
	"launch": { "desktopFiles": [ "kingpin-native.desktop" ], "installDir": "Games/Kingpin", "program": "run-game.sh", "requiredFiles": [ "kingpin.x86" ] } })json" };

constexpr std::string_view FormatThreeDownload{ R"json({ "format": 3, "name": "Kingpin", "launch": {
	"linux": { "desktopFiles": [ "kingpin-native.desktop" ], "installDir": "Games/Kingpin", "program": "run-game.sh", "requiredFiles": [ "kingpin.x86" ] },
	"windows": { "steamAppId": 38430, "program": "kingpin.exe", "requiredFiles": [ "main/pak0.pak" ] } } })json" };

constexpr std::string_view WindowsFacts{ R"json({ "steamAppId": 38430, "program": "kingpin.exe", "requiredFiles": [ "main/pak0.pak" ] })json" };

//////////////////////////////////////////////////////////////////////////
JsonValue Apply(std::string_view base, std::string_view patch)
{
	std::expected<std::string, std::string> const applied{ ApplyPatch(base, patch) };

	EXPECT_TRUE(applied.has_value()) << applied.error();

	return applied.has_value() ? JsonValue::parse(*applied) : JsonValue{};
}

//////////////////////////////////////////////////////////////////////////
JsonValue Make(std::string_view base, JsonValue const& game)
{
	std::expected<std::optional<std::string>, std::string> const made{ MakePatch(base, game.dump()) };

	EXPECT_TRUE(made.has_value() && made->has_value());

	return (made.has_value() && made->has_value()) ? JsonValue::parse(**made) : JsonValue{};
}

//////////////////////////////////////////////////////////////////////////
JsonValue MakeFormatThreeGame(std::string_view base)
{
	JsonValue game = JsonValue::parse(base);
	JsonValue facts = game["launch"];

	game["format"] = 3;
	game["launch"] = JsonValue::object();
	game["launch"]["linux"] = facts;
	game["launch"]["windows"] = JsonValue::parse(WindowsFacts);

	return game;
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, FormatOneChangeAppliesToAFormatThreeDownload)
{
	JsonValue game = Apply(FormatThreeDownload, R"json({ "launch": { "installDir": "Games/Mine" } })json");

	EXPECT_EQ(game["format"], 3);
	EXPECT_EQ(game["launch"]["linux"]["installDir"], "Games/Mine");
	EXPECT_EQ(game["launch"]["windows"]["steamAppId"], 38430);
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, FormatOneOfAChangeGivesWayToTheDownloadsFormatThree)
{
	JsonValue game = Apply(FormatThreeDownload, R"json({ "format": 1, "name": "Kingpin, mine" })json");

	EXPECT_EQ(game["format"], 3);
	EXPECT_EQ(game["name"], "Kingpin, mine");
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, FormatOneOfAChangeGivesWayWhenTheDownloadComesToNeedThree)
{
	constexpr std::string_view DesktopOnlyDownload{ R"json({ "format": 3, "name": "Kingpin", "launch": { "linux": { "desktopFiles": [ "kingpin-native.desktop" ] } } })json" };
	JsonValue const completed = JsonValue::parse(R"json({ "format": 1, "name": "Kingpin", "launch": { "desktopFiles": [ "kingpin-native.desktop" ],
		"installDir": "Games/Kingpin", "program": "run-game.sh", "requiredFiles": [] } })json");
	JsonValue const patch = Make(DesktopOnlyDownload, completed);
	JsonValue download = JsonValue::parse(DesktopOnlyDownload);

	download["launch"]["windows"] = JsonValue::parse(WindowsFacts);

	JsonValue game = Apply(download.dump(), patch.dump());

	EXPECT_EQ(game["format"], 3);
	EXPECT_EQ(game["launch"]["linux"]["installDir"], "Games/Kingpin");
	EXPECT_EQ(game["launch"]["windows"]["steamAppId"], 38430);
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, WholeFormatOneCopyKeepsTheDownloadsWindowsFacts)
{
	JsonValue copy = JsonValue::parse(FormatOneDownload);

	copy["launch"]["installDir"] = "Games/Mine";

	JsonValue game = Apply(FormatThreeDownload, copy.dump());

	EXPECT_EQ(game["format"], 3);
	EXPECT_EQ(game["launch"]["linux"]["installDir"], "Games/Mine");
	EXPECT_EQ(game["launch"]["windows"], JsonValue::parse(WindowsFacts));
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, EditedFormatOneLaunchMakesAFormatOnePatch)
{
	JsonValue game = JsonValue::parse(FormatOneDownload);

	game["launch"]["installDir"] = "Games/Mine";

	EXPECT_EQ(Make(FormatOneDownload, game), JsonValue::parse(R"json({ "launch": { "installDir": "Games/Mine" } })json"));
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, WindowsFactsAddedToAFormatOneDownloadMakeAPatchOfThemAlone)
{
	JsonValue patch = Make(FormatOneDownload, MakeFormatThreeGame(FormatOneDownload));
	JsonValue expected = JsonValue::parse(R"json({ "format": 3, "launch": {} })json");

	expected["launch"]["windows"] = JsonValue::parse(WindowsFacts);

	EXPECT_EQ(patch, expected);
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, ThatPatchAppliesToTheFormatOneDownload)
{
	JsonValue patch = Make(FormatOneDownload, MakeFormatThreeGame(FormatOneDownload));
	JsonValue game = Apply(FormatOneDownload, patch.dump());

	EXPECT_EQ(game, MakeFormatThreeGame(FormatOneDownload));
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, ThatPatchTakesTheLinuxFactsOfAFormatThreeDownload)
{
	JsonValue base = JsonValue::parse(FormatThreeDownload);

	base["launch"]["linux"]["installDir"] = "Games/KingpinUpdated";
	base["launch"]["windows"].erase("steamAppId");
	base["launch"]["windows"]["gogId"] = 1207658912;

	JsonValue patch = Make(FormatOneDownload, MakeFormatThreeGame(FormatOneDownload));
	JsonValue game = Apply(base.dump(), patch.dump());

	EXPECT_EQ(game["launch"]["linux"]["installDir"], "Games/KingpinUpdated");
	EXPECT_EQ(game["launch"]["windows"]["steamAppId"], 38430);
	EXPECT_EQ(game["launch"]["windows"]["gogId"], 1207658912);
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, EditedFormatThreeDownloadMakesAPlainPatch)
{
	JsonValue game = JsonValue::parse(FormatThreeDownload);

	game["launch"]["windows"]["program"] = "kingpin2.exe";

	EXPECT_EQ(Make(FormatThreeDownload, game), JsonValue::parse(R"json({ "launch": { "windows": { "program": "kingpin2.exe" } } })json"));
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, RemovedWindowsFactsMakeAFormatOnePatch)
{
	EXPECT_EQ(Make(FormatThreeDownload, JsonValue::parse(FormatOneDownload)), JsonValue::parse(R"json({ "format": 1, "launch": { "windows": null } })json"));
}

//////////////////////////////////////////////////////////////////////////
TEST(MergePatch, ThatPatchFlattensTheDownload)
{
	JsonValue game = Apply(FormatThreeDownload, R"json({ "format": 1, "launch": { "windows": null } })json");

	EXPECT_EQ(game, JsonValue::parse(FormatOneDownload));
}
} // namespace
} // namespace Lkt::Games
