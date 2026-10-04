#include "games/check_folder.hpp"
#include "layer_name.hpp"
#include "user_file_size.hpp"
#include "games/game_files.hpp"
#include "games/load_games.hpp"
#include "json/files.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

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
// A card's icon is two text lines high, about 80 px at a UI scale of 2.
constexpr uint32_t MinIconSize{ 128 };
constexpr std::array<std::string_view, 3> GameFiles{ "game.json", "icon.png", "icon-licence.txt" };

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
void CheckIcon(std::filesystem::path const& path, std::string_view shownAs, std::vector<Query::SGameProblem>& problems)
{
	std::expected<std::string, std::error_code> const icon{ Json::ReadFile(path, MaxUserFileSize) };
	std::optional<std::array<uint32_t, 2>> const size{ icon.has_value() ? ReadPngSize(std::as_bytes(std::span{ *icon })) : std::nullopt };

	if (!icon.has_value())
	{
		problems.emplace_back(Query::SGameProblem{ std::format("{}: {}", shownAs, icon.error().message()), {} });
	}
	else if (!size.has_value())
	{
		problems.emplace_back(Query::SGameProblem{ std::format("{}: is not a PNG", shownAs), {} });
	}
	else if ((*size)[0] != (*size)[1])
	{
		problems.emplace_back(Query::SGameProblem{ std::format("{}: is {} x {} px; an icon is square", shownAs, (*size)[0], (*size)[1]), {} });
	}
	else if ((*size)[0] < MinIconSize)
	{
		problems.emplace_back(Query::SGameProblem{ std::format("{}: is {} px; an icon is at least {} px", shownAs, (*size)[0], MinIconSize), {} });
	}
}

//////////////////////////////////////////////////////////////////////////
void CheckGameFiles(std::filesystem::path const& gameFolder, std::string_view shownAs, std::vector<Query::SGameProblem>& problems)
{
	std::error_code error{};
	bool const hasIcon{ std::filesystem::exists(gameFolder / "icon.png", error) };
	bool const hasLicence{ std::filesystem::exists(gameFolder / "icon-licence.txt", error) };

	for (std::filesystem::directory_iterator it{ gameFolder, error }, end{}; error.value() == 0 && it != end; it.increment(error))
	{
		std::string const name{ it->path().filename().string() };

		if (!std::ranges::contains(GameFiles, name))
		{
			problems.emplace_back(Query::SGameProblem{ std::format("{}/{}: is not a file a game holds", shownAs, name), {} });
		}
	}

	if (hasIcon && !hasLicence)
	{
		problems.emplace_back(Query::SGameProblem{ std::format("{}/icon.png: has no icon-licence.txt beside it", shownAs), {} });
	}
	else if (hasLicence && !hasIcon)
	{
		problems.emplace_back(Query::SGameProblem{ std::format("{}/icon-licence.txt: has no icon.png to describe", shownAs), {} });
	}

	if (hasIcon)
	{
		CheckIcon(gameFolder / "icon.png", std::format("{}/icon.png", shownAs), problems);
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
SGameContent CheckGameFolder(std::filesystem::path const& folder)
{
	SGameContent content{ LoadGames(folder, {}) };
	std::string const layer{ NameLayer(folder) };
	std::error_code error{};
	bool const hasGames{ std::filesystem::is_directory(folder / "games", error) };

	if (!hasGames && !std::filesystem::is_directory(folder / "protocols", error))
	{
		content.problems.emplace_back(Query::SGameProblem{ std::format("{}: holds neither games/ nor protocols/", layer), {} });
	}

	if (hasGames)
	{
		for (std::filesystem::directory_iterator it{ folder / "games", error }, end{}; error.value() == 0 && it != end; it.increment(error))
		{
			std::string const key{ it->path().filename().string() };

			if (it->is_directory(error) && IsValidKey(key))
			{
				CheckGameFiles(it->path(), std::format("{}/games/{}", layer, key), content.problems);
			}
		}
	}

	return content;
}
} // namespace Lkt::Games
