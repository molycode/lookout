#include "embedded_games.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace Lkt::Games
{
namespace
{
constexpr std::array<unsigned char, 8> PngSignature{ 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
constexpr std::string_view HeaderChunk{ "IHDR" };
constexpr size_t ChunkTypeOffset{ 12 };
constexpr size_t WidthOffset{ 16 };
constexpr size_t HeightOffset{ 20 };
constexpr size_t HeaderEnd{ 33 };
constexpr uint32_t MinIconSize{ 128 };

//////////////////////////////////////////////////////////////////////////
uint32_t ReadBigEndian(std::span<std::byte const> bytes, size_t offset)
{
	uint32_t value{ 0 };

	for (std::byte const byte : bytes.subspan(offset, 4))
	{
		value = (value << 8) | static_cast<uint32_t>(byte);
	}

	return value;
}

//////////////////////////////////////////////////////////////////////////
// Width and height, when the bytes start like a PNG.
std::optional<std::array<uint32_t, 2>> ReadPngSize(std::span<std::byte const> png)
{
	std::optional<std::array<uint32_t, 2>> size{};

	if (png.size() >= HeaderEnd && std::ranges::equal(png.first(PngSignature.size()), std::as_bytes(std::span{ PngSignature }))
		&& std::ranges::equal(std::as_bytes(std::span{ HeaderChunk }), png.subspan(ChunkTypeOffset, HeaderChunk.size())))
	{
		size = std::array<uint32_t, 2>{ ReadBigEndian(png, WidthOffset), ReadBigEndian(png, HeightOffset) };
	}

	return size;
}

//////////////////////////////////////////////////////////////////////////
TEST(BuiltinIcons, EveryGameHasOne)
{
	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		EXPECT_FALSE(game.icon.empty()) << game.key;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(BuiltinIcons, EveryIconBelongsToAGame)
{
	for (Embedded::SEmbeddedFile const& file : Embedded::GameIcons)
	{
		EXPECT_NE(Query::FindGame(file.name.substr(0, file.name.find('/'))), nullptr) << file.name;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(BuiltinIcons, EveryIconIsAPng)
{
	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		EXPECT_TRUE(ReadPngSize(game.icon).has_value()) << game.key;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(BuiltinIcons, EveryIconIsSquare)
{
	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		std::array<uint32_t, 2> const size{ ReadPngSize(game.icon).value_or(std::array<uint32_t, 2>{ 0, 1 }) };

		EXPECT_EQ(size[0], size[1]) << game.key;
	}
}

//////////////////////////////////////////////////////////////////////////
// A card's icon is two text lines high, about 80 px at a UI scale of 2.
TEST(BuiltinIcons, EveryIconIsLargeEnoughForHighDensityScreens)
{
	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		EXPECT_GE(ReadPngSize(game.icon).value_or(std::array<uint32_t, 2>{ 0, 0 })[0], MinIconSize) << game.key;
	}
}
} // namespace
} // namespace Lkt::Games
