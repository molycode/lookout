#include "games/game_fields.hpp"
#include "games/game_format.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <format>
#include <map>
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

//////////////////////////////////////////////////////////////////////////
TEST(GameFields, EveryFieldIsOfAFormatThisLookoutReads)
{
	for (SGameField const& field : GetGameFields())
	{
		EXPECT_GE(field.firstFormat, FirstGameFormat) << field.parent << " " << field.name;
		EXPECT_LE(field.firstFormat, GameFormat) << field.parent << " " << field.name;
	}
}

//////////////////////////////////////////////////////////////////////////
// An older format could not hold the field, having no place to put it.
TEST(GameFields, NoFieldIsOlderThanItsGroup)
{
	std::map<std::string, uint64_t> groupFormats{ { "", FirstGameFormat } };

	for (SGameField const& field : GetGameFields())
	{
		std::string const path{ field.parent.empty() ? std::string{ field.name } : std::format("{}.{}", field.parent, field.name) };

		EXPECT_GE(field.firstFormat, groupFormats[std::string{ field.parent }]) << path;
		groupFormats[(field.kind == EGameFieldKind::GroupList) ? std::format("{}[]", path) : path] = field.firstFormat;
	}
}

//////////////////////////////////////////////////////////////////////////
// A required field of a later format would make every earlier file wrong.
TEST(GameFields, FieldOfALaterFormatIsNeverRequired)
{
	for (SGameField const& field : GetGameFields())
	{
		EXPECT_TRUE(field.firstFormat == FirstGameFormat || !field.isRequired) << field.parent << " " << field.name;
	}
}
} // namespace
} // namespace Lkt::Games
