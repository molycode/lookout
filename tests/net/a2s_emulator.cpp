#include "net/a2s_emulator.hpp"
#include "fixtures.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <tuple>
#include <utility>

namespace Lkt::Fixtures
{
namespace
{
constexpr std::string_view Single{ "\xFF\xFF\xFF\xFF" };
constexpr std::string_view Split{ "\xFE\xFF\xFF\xFF" };
constexpr std::string_view NoChallenge{ "\xFF\xFF\xFF\xFF" };
constexpr std::string_view Challenge{ "\x11\x22\x33\x44" };
constexpr std::string_view InfoQuery{ "TSource Engine Query\0", 21 };
constexpr std::string_view MasterReplyHeader{ "\xFF\xFF\xFF\xFF" "f\n" };
constexpr uint32_t SplitId{ 0x1234 };
constexpr uint16_t SplitSize{ 1248 };
constexpr uint32_t NumRules{ 60 };
// Sent in this order, so the script must put them back together itself.
constexpr std::array<size_t, 3> FragmentOrder{ 2, 0, 1 };

//////////////////////////////////////////////////////////////////////////
void Append(std::vector<std::byte>& bytes, std::string_view text)
{
	for (char const c : text)
	{
		bytes.emplace_back(static_cast<std::byte>(c));
	}
}

//////////////////////////////////////////////////////////////////////////
void AppendString(std::vector<std::byte>& bytes, std::string_view text)
{
	Append(bytes, text);
	bytes.emplace_back(std::byte{ 0 });
}

//////////////////////////////////////////////////////////////////////////
void AppendLittleEndian(std::vector<std::byte>& bytes, uint64_t value, size_t size)
{
	for (size_t index{ 0 }; index < size; ++index)
	{
		bytes.emplace_back(static_cast<std::byte>(value >> (8 * index)));
	}
}

//////////////////////////////////////////////////////////////////////////
void AppendBigEndian(std::vector<std::byte>& bytes, uint64_t value, size_t size)
{
	for (size_t index{ size }; index > 0; --index)
	{
		bytes.emplace_back(static_cast<std::byte>(value >> (8 * (index - 1))));
	}
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeQuery(std::string_view kind, std::string_view challenge)
{
	std::vector<std::byte> query{};

	Append(query, Single);
	Append(query, kind);
	Append(query, challenge);

	return query;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeChallenge()
{
	return MakeQuery("A", Challenge);
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeInfo(SA2sServerSetup const& setup)
{
	std::vector<std::byte> info{ MakeQuery("I", {}) };

	AppendLittleEndian(info, 17, 1);
	AppendString(info, A2sServerName);
	AppendString(info, "cp_badlands");
	AppendString(info, "tf");
	AppendString(info, "Team Fortress");
	AppendLittleEndian(info, 440, 2);
	AppendLittleEndian(info, 2, 1);
	AppendLittleEndian(info, 24, 1);
	AppendLittleEndian(info, 0, 1);
	Append(info, "dl");
	AppendLittleEndian(info, setup.hasPassword ? 1 : 0, 1);
	AppendLittleEndian(info, 1, 1);
	AppendString(info, "1.0.0.0");
	AppendLittleEndian(info, 0x80 | 0x40 | 0x20 | 0x10 | 0x01, 1);
	AppendLittleEndian(info, setup.joinPort, 2);
	AppendLittleEndian(info, 0x0110000100000001, 8);
	AppendLittleEndian(info, 27020, 2);
	AppendString(info, "SourceTV");
	AppendString(info, "alltalk,nocrits");
	AppendLittleEndian(info, 440, 8);

	return info;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakePlayers()
{
	std::vector<std::byte> players{ MakeQuery("D", {}) };

	AppendLittleEndian(players, 2, 1);

	for (auto const& [index, name, score, duration] : { std::tuple{ 0, "Alice", 10, 123.5f }, std::tuple{ 1, "Bob", -2, 60.0f } })
	{
		AppendLittleEndian(players, static_cast<uint64_t>(index), 1);
		AppendString(players, name);
		AppendLittleEndian(players, static_cast<uint32_t>(score), 4);
		AppendLittleEndian(players, std::bit_cast<uint32_t>(duration), 4);
	}

	return players;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::vector<std::byte>> MakeRulesFragments()
{
	std::vector<std::byte> rules{ MakeQuery("E", {}) };

	AppendLittleEndian(rules, NumRules, 2);

	for (uint32_t index{ 0 }; index < NumRules; ++index)
	{
		AppendString(rules, std::format("rule{}", index));
		AppendString(rules, std::format("value{}", index));
	}

	size_t const partSize{ (rules.size() + FragmentOrder.size() - 1) / FragmentOrder.size() };
	std::vector<std::vector<std::byte>> fragments{};

	for (size_t const number : FragmentOrder)
	{
		std::vector<std::byte>& fragment{ fragments.emplace_back() };
		size_t const start{ number * partSize };

		Append(fragment, Split);
		AppendLittleEndian(fragment, SplitId, 4);
		AppendLittleEndian(fragment, FragmentOrder.size(), 1);
		AppendLittleEndian(fragment, number, 1);
		AppendLittleEndian(fragment, SplitSize, 2);
		fragment.insert(fragment.end(), rules.begin() + static_cast<std::ptrdiff_t>(start),
			rules.begin() + static_cast<std::ptrdiff_t>(std::min(start + partSize, rules.size())));
	}

	return fragments;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakePageRequest(std::string_view seed)
{
	std::vector<std::byte> request{};

	Append(request, "1\xFF");
	AppendString(request, seed);
	AppendString(request, A2sFilter);

	return request;
}

//////////////////////////////////////////////////////////////////////////
void AppendEntry(std::vector<std::byte>& page, Query::SServerAddress const& server)
{
	AppendBigEndian(page, server.ipv4, 4);
	AppendBigEndian(page, server.port, 2);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::vector<SLoopbackExchange> MakeA2sServer(SA2sServerSetup const& setup)
{
	std::vector<SLoopbackExchange> exchanges{};

	exchanges.emplace_back(MakeQuery(InfoQuery, {}), std::vector<std::vector<std::byte>>{ setup.isChallenging ? MakeChallenge() : MakeInfo(setup) });
	exchanges.emplace_back(MakeQuery(InfoQuery, Challenge), std::vector<std::vector<std::byte>>{ MakeInfo(setup) }, setup.dropsFirstInfo ? 1u : 0u);
	exchanges.emplace_back(MakeQuery("U", NoChallenge), std::vector<std::vector<std::byte>>{ MakeChallenge() });
	exchanges.emplace_back(MakeQuery("U", Challenge), std::vector<std::vector<std::byte>>{ MakePlayers() });
	exchanges.emplace_back(MakeQuery("V", NoChallenge), std::vector<std::vector<std::byte>>{ MakeChallenge() });
	exchanges.emplace_back(MakeQuery("V", Challenge), setup.sendsRules ? MakeRulesFragments() : std::vector<std::vector<std::byte>>{});

	return exchanges;
}

//////////////////////////////////////////////////////////////////////////
std::vector<SLoopbackExchange> MakeSteamMaster(Query::SServerAddress const& firstPage, Query::SServerAddress const& secondPage)
{
	std::vector<std::byte> first{};
	std::vector<std::byte> second{};

	Append(first, MasterReplyHeader);
	AppendEntry(first, firstPage);
	Append(second, MasterReplyHeader);
	AppendEntry(second, secondPage);
	AppendEntry(second, Query::SServerAddress{ 0, 0 });

	std::vector<SLoopbackExchange> exchanges{};

	exchanges.emplace_back(MakePageRequest("0.0.0.0:0"), std::vector<std::vector<std::byte>>{ std::move(first) });
	exchanges.emplace_back(MakePageRequest(Query::FormatAddress(firstPage)), std::vector<std::vector<std::byte>>{ std::move(second) }, 1u);

	return exchanges;
}
} // namespace Lkt::Fixtures
