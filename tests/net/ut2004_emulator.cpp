#include "net/ut2004_emulator.hpp"
#include "fixtures.hpp"
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace Lkt::Fixtures
{
namespace
{
constexpr std::string_view QueryHeader{ "\x79\x00\x00\x00", 4 };
constexpr std::string_view ReplyHeader{ "\x80\x00\x00\x00", 4 };
constexpr std::string_view RawName{ "\x1B\xFF\x80\x01Red \x1B\x01\x01\xFF" "Blu\xE9" };
constexpr std::u16string_view WideAdminName{ u"Ünïcødé ✓ \U0001F600" };
constexpr std::u16string_view WidePlayer{ u"Böb✓" };
constexpr std::string_view Challenge{ "1a2b3c4d" };
constexpr std::string_view KeyHash{ "00000000000000000000000000000000" };
constexpr uint32_t ClientVersion{ 3369 };
constexpr uint32_t RedTeamBit{ 0x20000000 };
constexpr uint32_t BlueTeamBit{ 0x40000000 };
constexpr size_t OddWriteSize{ 5 };

//////////////////////////////////////////////////////////////////////////
void Append(std::vector<std::byte>& bytes, std::string_view text)
{
	for (char const c : text)
	{
		bytes.emplace_back(static_cast<std::byte>(c));
	}
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
// The sign in bit 7 and a continuation in bit 6 of the first byte, which holds the low 6 bits; 7 bits a byte after.
void AppendCompactIndex(std::vector<std::byte>& bytes, int32_t value)
{
	uint32_t magnitude{ static_cast<uint32_t>((value < 0) ? -value : value) };
	uint32_t first{ ((value < 0) ? 0x80u : 0u) | (magnitude & 0x3F) };

	magnitude >>= 6;
	bytes.emplace_back(static_cast<std::byte>(first | ((magnitude != 0) ? 0x40u : 0u)));

	while (magnitude != 0)
	{
		uint32_t const next{ magnitude & 0x7F };

		magnitude >>= 7;
		bytes.emplace_back(static_cast<std::byte>(next | ((magnitude != 0) ? 0x80u : 0u)));
	}
}

//////////////////////////////////////////////////////////////////////////
void AppendString(std::vector<std::byte>& bytes, std::string_view latin1)
{
	AppendCompactIndex(bytes, static_cast<int32_t>(latin1.size() + 1));
	Append(bytes, latin1);
	bytes.emplace_back(std::byte{ 0 });
}

//////////////////////////////////////////////////////////////////////////
void AppendWideString(std::vector<std::byte>& bytes, std::u16string_view text)
{
	AppendCompactIndex(bytes, -static_cast<int32_t>(text.size() + 1));

	for (char16_t const unit : text)
	{
		AppendLittleEndian(bytes, unit, 2);
	}

	AppendLittleEndian(bytes, 0, 2);
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakePacket(uint8_t command)
{
	std::vector<std::byte> packet{};

	Append(packet, ReplyHeader);
	AppendLittleEndian(packet, command, 1);

	return packet;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeInfo(SUt2004ServerSetup const& setup)
{
	std::vector<std::byte> info{ MakePacket(0) };

	AppendLittleEndian(info, 0, 4);
	AppendCompactIndex(info, 0);
	AppendLittleEndian(info, setup.joinPort, 4);
	AppendLittleEndian(info, 0, 4);
	AppendString(info, RawName);
	AppendString(info, Ut2004Map);
	AppendString(info, "xDeathMatch");
	AppendLittleEndian(info, setup.hasPlayers ? 2 : 0, 4);
	AppendLittleEndian(info, 16, 4);
	AppendLittleEndian(info, 0, 4);
	AppendLittleEndian(info, 0, 4);
	AppendString(info, "1");
	AppendLittleEndian(info, 0, 2);

	return info;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::vector<std::byte>> MakeRules(SUt2004ServerSetup const& setup)
{
	std::vector<std::byte> first{ MakePacket(1) };
	std::vector<std::byte> second{ MakePacket(1) };

	AppendString(first, "ServerMode");
	AppendString(first, "dedicated");
	AppendString(first, "AdminName");
	AppendWideString(first, WideAdminName);

	if (setup.hasPassword)
	{
		AppendString(first, "GamePassword");
		AppendString(first, "True");
	}

	AppendString(second, "MOTD");
	AppendString(second, std::string(Ut2004MotdLength, 'm'));
	AppendString(second, "Mutator");
	AppendString(second, "MutInstaGib");

	return { std::move(first), std::move(second) };
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> EncodeString(std::string_view latin1)
{
	std::vector<std::byte> bytes{};

	AppendString(bytes, latin1);

	return bytes;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> EncodeWideString(std::u16string_view text)
{
	std::vector<std::byte> bytes{};

	AppendWideString(bytes, text);

	return bytes;
}

//////////////////////////////////////////////////////////////////////////
void AppendPlayer(std::vector<std::byte>& packet, int32_t id, std::vector<std::byte> const& name, int32_t ping, int32_t score, uint32_t statsId)
{
	AppendLittleEndian(packet, static_cast<uint32_t>(id), 4);
	packet.insert(packet.end(), name.begin(), name.end());
	AppendLittleEndian(packet, static_cast<uint32_t>(ping), 4);
	AppendLittleEndian(packet, static_cast<uint32_t>(score), 4);
	AppendLittleEndian(packet, statsId, 4);
}

//////////////////////////////////////////////////////////////////////////
// With a team label of the kind mutators add, which is no player.
std::vector<std::vector<std::byte>> MakePlayers()
{
	std::vector<std::byte> first{ MakePacket(2) };
	std::vector<std::byte> second{ MakePacket(2) };

	AppendPlayer(first, 1, EncodeString("Alice"), 40, 10, RedTeamBit);
	AppendPlayer(first, 0, EncodeString("\x1B\xFF\x01\x01Red"), 0, 0, 0);
	AppendPlayer(second, 2, EncodeWideString(WidePlayer), 60, -3, BlueTeamBit);

	return { std::move(first), std::move(second) };
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeQuery(uint8_t command)
{
	std::vector<std::byte> query{};

	Append(query, QueryHeader);
	AppendLittleEndian(query, command, 1);

	return query;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeFrame(std::vector<std::byte> const& payload)
{
	std::vector<std::byte> frame{};

	AppendLittleEndian(frame, payload.size(), 4);
	frame.insert(frame.end(), payload.begin(), payload.end());

	return frame;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeClientResponse()
{
	std::vector<std::byte> response{};

	AppendString(response, KeyHash);
	AppendString(response, KeyHash);
	AppendString(response, "UT2K4CLIENT");
	AppendLittleEndian(response, ClientVersion, 4);
	AppendLittleEndian(response, 0, 1);
	AppendString(response, "int");
	AppendLittleEndian(response, 0, 4);
	AppendLittleEndian(response, 0, 4);
	AppendLittleEndian(response, 0, 4);
	AppendLittleEndian(response, 0, 1);

	return MakeFrame(response);
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeStringFrame(std::string_view text)
{
	std::vector<std::byte> payload{};

	AppendString(payload, text);

	return MakeFrame(payload);
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeList(std::span<Query::SServerAddress const> servers)
{
	std::vector<std::byte> count{};

	AppendLittleEndian(count, servers.size(), 4);
	AppendLittleEndian(count, 1, 1);

	std::vector<std::byte> list{ MakeFrame(count) };

	for (Query::SServerAddress const& server : servers)
	{
		std::vector<std::byte> entry{};

		for (int shift{ 24 }; shift >= 0; shift -= 8)
		{
			entry.emplace_back(static_cast<std::byte>(server.ipv4 >> shift));
		}

		AppendLittleEndian(entry, server.port - 1u, 2);
		AppendLittleEndian(entry, server.port, 2);
		AppendString(entry, "Listed");
		AppendString(entry, Ut2004Map);
		AppendString(entry, "xDeathMatch");
		AppendLittleEndian(entry, 2, 1);
		AppendLittleEndian(entry, 16, 1);
		AppendLittleEndian(entry, 0, 4);
		AppendString(entry, "1");

		std::vector<std::byte> const frame{ MakeFrame(entry) };

		list.insert(list.end(), frame.begin(), frame.end());
	}

	return list;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::vector<SLoopbackExchange> MakeUt2004Server(SUt2004ServerSetup const& setup)
{
	std::vector<SLoopbackExchange> exchanges{};

	exchanges.emplace_back(MakeQuery(0), std::vector<std::vector<std::byte>>{ MakeInfo(setup) }, 0u, setup.packetInterval);
	exchanges.emplace_back(MakeQuery(1), MakeRules(setup), 0u, setup.packetInterval);
	exchanges.emplace_back(MakeQuery(2), setup.hasPlayers ? MakePlayers() : std::vector<std::vector<std::byte>>{}, 0u, setup.packetInterval);

	return exchanges;
}

//////////////////////////////////////////////////////////////////////////
SLoopbackStream MakeUt2004Master(std::span<Query::SServerAddress const> servers, std::chrono::milliseconds listPause)
{
	std::vector<std::byte> approval{};

	AppendString(approval, "APPROVED");
	AppendLittleEndian(approval, 3, 4);

	std::vector<SLoopbackStreamStep> steps{};

	steps.emplace_back(std::vector<std::byte>{}, MakeStringFrame(Challenge));
	steps.emplace_back(MakeClientResponse(), MakeFrame(approval));
	steps.emplace_back(MakeStringFrame(KeyHash), MakeStringFrame("VERIFIED"));
	steps.emplace_back(MakeFrame(std::vector<std::byte>{ std::byte{ 0 }, std::byte{ 0 } }), MakeList(servers), listPause);

	return SLoopbackStream{ std::move(steps), OddWriteSize, true };
}
} // namespace Lkt::Fixtures
