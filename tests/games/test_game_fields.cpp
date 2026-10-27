#include "games/game_fields.hpp"
#include <gtest/gtest.h>
#include <format>
#include <set>
#include <string>

namespace Lkt::Games
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(GameFields, EveryFieldHasALabelAndADescription)
{
	for (SGameField const& field : GetGameFields())
	{
		EXPECT_FALSE(field.label.empty()) << field.parent << " " << field.name;
		EXPECT_FALSE(field.description.empty()) << field.parent << " " << field.name;
	}
}

//////////////////////////////////////////////////////////////////////////
// The form finds a field by walking from the groups before it.
TEST(GameFields, EveryNestedFieldBelongsToAGroupListedBeforeIt)
{
	std::set<std::string> parents{ "" };

	for (SGameField const& field : GetGameFields())
	{
		std::string const path{ field.parent.empty() ? std::string{ field.name } : std::format("{}.{}", field.parent, field.name) };

		EXPECT_TRUE(parents.contains(std::string{ field.parent })) << path;

		if (field.kind == EGameFieldKind::Group)
		{
			parents.insert(path);
		}
		else if (field.kind == EGameFieldKind::GroupList)
		{
			parents.insert(std::format("{}[]", path));
		}
	}
}
} // namespace
} // namespace Lkt::Games
