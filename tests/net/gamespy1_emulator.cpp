#include "net/gamespy1_emulator.hpp"
#include "fixtures.hpp"
#include <cstddef>
#include <format>
#include <utility>
#include <vector>

namespace Lkt::Fixtures
{
namespace
{
constexpr size_t OddWriteSize{ 3 };
} // namespace

//////////////////////////////////////////////////////////////////////////
// FL2fKXOg is gsmsalg of the challenge under the gspylite key, as tools/query.py computes it and both masters accept.
std::string MakeGamespy1ListRequest(std::string_view masterGame)
{
	return std::format("\\gamename\\gspylite\\location\\0\\validate\\FL2fKXOg\\final\\\\list\\\\gamename\\{}\\final\\", masterGame);
}

//////////////////////////////////////////////////////////////////////////
SLoopbackStream MakeGamespy1Master(std::string_view masterGame, std::span<Query::SServerAddress const> servers)
{
	std::string list{};

	for (Query::SServerAddress const& server : servers)
	{
		list += std::format("\\ip\\{}", Query::FormatAddress(server));
	}

	list += "\\final\\";

	std::vector<SLoopbackStreamStep> steps{};

	steps.emplace_back(std::vector<std::byte>{}, ToBytes(Gamespy1Challenge));
	steps.emplace_back(ToBytes(MakeGamespy1ListRequest(masterGame)), ToBytes(list));

	return SLoopbackStream{ std::move(steps), OddWriteSize, true };
}
} // namespace Lkt::Fixtures
