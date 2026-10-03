#include "desktop_entry.hpp"
#include <gtest/gtest.h>
#include <string_view>

namespace Lkt::Launch
{
namespace
{
// As kp-dev's installer writes it.
constexpr std::string_view KingpinEntry{ R"([Desktop Entry]
Type=Application
Name=Kingpin: Life of Crime
Comment=Native Linux, no Wine or Proton
Exec="/opt/games/Kingpin/run-game.sh"
Path=/opt/games/Kingpin
Icon=/opt/games/Kingpin/kingpin.png
Terminal=false
Categories=Game;ActionGame;
StartupWMClass=Kingpin
)" };

//////////////////////////////////////////////////////////////////////////
SDesktopEntry ParseValid(std::string_view text)
{
	std::expected<SDesktopEntry, EDesktopEntryError> const entry{ ParseDesktopEntry(text) };

	EXPECT_TRUE(entry.has_value()) << text;

	return entry.value_or(SDesktopEntry{});
}

//////////////////////////////////////////////////////////////////////////
TEST(DesktopEntry, ReadsAnInstalledEntry)
{
	SDesktopEntry const entry{ ParseValid(KingpinEntry) };

	EXPECT_EQ(entry.type, "Application");
	EXPECT_EQ(entry.name, "Kingpin: Life of Crime");
	EXPECT_EQ(entry.exec, "\"/opt/games/Kingpin/run-game.sh\"");
	EXPECT_EQ(entry.path, "/opt/games/Kingpin");
	EXPECT_TRUE(entry.tryExec.empty());
	EXPECT_FALSE(entry.isHidden);
}

//////////////////////////////////////////////////////////////////////////
TEST(DesktopEntry, CommentsOtherGroupsAndLocalisedKeysAreIgnored)
{
	SDesktopEntry const entry{ ParseValid("# written by hand\n[Desktop Entry]\nName=Kingpin\nName[de]=Ganove\n# Exec=commented.sh\n"
		"[Desktop Action Server]\nExec=run-server.sh\n") };

	EXPECT_EQ(entry.name, "Kingpin");
	EXPECT_TRUE(entry.exec.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(DesktopEntry, LeadingBlanksSpacesAroundEqualsAndCarriageReturnsAreStripped)
{
	SDesktopEntry const entry{ ParseValid("[Desktop Entry]\r\n  Name = Kingpin\r\n\tExec=run-game.sh\r\n") };

	EXPECT_EQ(entry.name, "Kingpin");
	EXPECT_EQ(entry.exec, "run-game.sh");
}

//////////////////////////////////////////////////////////////////////////
TEST(DesktopEntry, StringEscapesAreDecoded)
{
	EXPECT_EQ(ParseValid("[Desktop Entry]\nName=Big\\sBad\\tKingpin\\\\\n").name, "Big Bad\tKingpin\\");
}

//////////////////////////////////////////////////////////////////////////
// GLib refuses the key, so the entry would not start from the desktop either.
TEST(DesktopEntry, UnknownEscapeFails)
{
	EXPECT_EQ(ParseDesktopEntry("[Desktop Entry]\nExec=run \\\"game\\\"\n").error_or(EDesktopEntryError::MalformedLine), EDesktopEntryError::UnknownEscape);
}

//////////////////////////////////////////////////////////////////////////
TEST(DesktopEntry, NumericBooleansAreRead)
{
	EXPECT_TRUE(ParseValid("[Desktop Entry]\nHidden=1\n").isHidden);
}

//////////////////////////////////////////////////////////////////////////
TEST(DesktopEntry, TrailingBlanksAfterABooleanAreIgnored)
{
	EXPECT_TRUE(ParseValid("[Desktop Entry]\nHidden=true \t\n").isHidden);
}

//////////////////////////////////////////////////////////////////////////
TEST(DesktopEntry, InvalidBooleanFails)
{
	EXPECT_EQ(ParseDesktopEntry("[Desktop Entry]\nHidden=yes\n").error_or(EDesktopEntryError::MalformedLine), EDesktopEntryError::InvalidBoolean);
}

//////////////////////////////////////////////////////////////////////////
TEST(DesktopEntry, LineWithoutKeyFails)
{
	EXPECT_EQ(ParseDesktopEntry("[Desktop Entry]\nKingpin\n").error_or(EDesktopEntryError::UnknownEscape), EDesktopEntryError::MalformedLine);
}

//////////////////////////////////////////////////////////////////////////
TEST(DesktopEntry, FileWithoutDesktopEntryGroupFails)
{
	EXPECT_EQ(ParseDesktopEntry("[Desktop Action Server]\nName=Kingpin\n").error_or(EDesktopEntryError::MalformedLine), EDesktopEntryError::NoDesktopEntryGroup);
}
} // namespace
} // namespace Lkt::Launch
