#!/usr/bin/env python3
"""Reference server query, independent of Lookout's C++ code.

    tools/query.py <game>                          list the game's servers, one line each
    tools/query.py <game> --save-fixtures <dir>    also save raw replies as parser test data

Each line is "<address> <players>/<max> <map> <name>", sorted by address, the address being the one queried. Compare
`lookout --list <game>` with it on the first three columns: names are only roughly decoded here.
"""

import argparse
import pathlib
import re
import select
import socket
import struct
import sys
import time

GAMES = {
	"kingpin": ("quake2", [("master.kingpin.info", 27900)], ""),
	"quake2": ("quake2", [("master.quakeservers.net", 27900), ("master.maraakate.org", 27900)], ""),
	"rtcw": ("quake3", [("wolfmaster.idsoftware.com", 27950)], "60 empty full"),
	"et": ("quake3", [("etmaster.idsoftware.com", 27950), ("etmaster.etlegacy.com", 27950)], "84 empty full"),
	"quake3": ("quake3", [("master.quake3arena.com", 27950), ("master.ioquake3.org", 27950), ("dpmaster.deathmask.net", 27950)], "68 empty full"),
	"ut2004": ("unreal2", [("ut2004master.333networks.com", 28902), ("utmaster.openspy.net", 28902)], ""),
	"urbanterror": ("quake3", [("master.urbanterror.info", 27900)], "68 empty full"),
	"jka": ("quake3", [("master.jkhub.org", 29060), ("master.jk2mv.org", 29060)], "26 empty full"),
	"jk2": ("quake3", [("master.jkhub.org", 28060), ("master.jk2mv.org", 28060), ("master.jk2.daggolin.de", 28060)], "15,16 empty full"),
	"sof2": ("quake3", [("master.1fxmod.org", 20110)], "2004 empty full"),
	"xonotic": ("quake3", [("dpmaster.deathmask.net", 27950), ("dpmaster.tchr.no", 27950)], "Xonotic 3 empty full"),
	"nexuiz": ("quake3", [("dpmaster.deathmask.net", 27950), ("dpmaster.tchr.no", 27950)], "Nexuiz 3 empty full"),
	"warsow": ("quake3", [("dpmaster.deathmask.net", 27950), ("dpmaster.tchr.no", 27950)], "Warsow 22 empty full"),
	"tremulous": ("quake3", [("master.tremulous.net", 30700)], "71 empty full"),
	"unvanquished": ("quake3", [("master.unvanquished.net", 27950), ("master2.unvanquished.net", 27950)], "86 empty full"),
	"smokinguns": ("quake3", [("master.smokin-guns.org", 27950)], "68 empty full"),
	"eliteforce": ("quake3", [("efmaster.tjps.eu", 27953), ("master.stef1.daggolin.de", 27953), ("master.stvef.org", 27953)], "24 empty full"),
	"cod": ("quake3", [("codmaster.activision.com", 20510)], "6 full empty"),
	"coduo": ("quake3", [("coduomaster.activision.com", 20610)], "22 full empty"),
	"cod2": ("quake3", [("cod2master.activision.com", 20710), ("master.cod2x.me", 20710)], "118,120 full empty"),
	"cod4": ("quake3", [("cod4master.activision.com", 20810)], "6 full empty"),
	"alienarena": ("quake2", [("master.alienarena.org", 27900), ("master2.alienarena.org", 27900)], ""),
}
# Elite Force's masters write each address as twelve hex digits.
HEX_ENTRIES = {"eliteforce"}

HEADER = b"\xff\xff\xff\xff"
NAME_KEYS = ("hostname", "sv_hostname")
MAX_KEYS = ("maxclients", "sv_maxclients")
MASTER_WAIT = 2.0
STATUS_WAIT = 1.5
IN_FLIGHT = 32

# Unreal Engine 2: TCP masters that speak first in frames of a u32 length, and servers that answer each command in
# packets with no count or end, so a server is done once quiet. No master checks the CD key hashes.
UNREAL2_KEY_HASH = "0" * 32
UNREAL2_QUERIES = [b"\x79\x00\x00\x00" + bytes([command]) for command in (0, 1, 2)]
UNREAL2_QUIET = 0.6
UNREAL2_INFO = 0


def master_requests(family, args):
	"""One request, or one per protocol number when they are joined by commas ("15,16 empty full")."""
	if family == "quake2":
		return [b"query"]

	words = args.split(" ")
	index = next((i for i, word in enumerate(words) if "," in word), None)
	queries = [args] if index is None else [" ".join(words[:index] + [number] + words[index + 1:]) for number in words[index].split(",")]
	return [HEADER + b"getservers " + query.encode() for query in queries]


def parse_master(family, packet, is_hex=False):
	servers = []

	if family == "quake2":
		if packet.startswith(HEADER + b"servers") and packet[len(HEADER) + 7:len(HEADER) + 8] in (b" ", b"\n"):
			body = packet[len(HEADER + b"servers") + 1:]
			servers = [(socket.inet_ntoa(body[i:i + 4]), struct.unpack(">H", body[i + 4:i + 6])[0]) for i in range(0, len(body) - 5, 6)]
	elif packet.startswith(HEADER + b"getserversResponse"):
		# Call of Duty's masters put "\n\0" before the first entry, JK2MV's "\n", Elite Force's a space.
		body = packet[len(HEADER + b"getserversResponse"):].lstrip(b"\n\0 ")
		size = 13 if is_hex else 7
		pos = 0

		# An end marker ends a datagram only when nothing but padding follows: 69.79.84.x spells the same bytes.
		while pos + size <= len(body) and not (body[pos:pos + 4] in (b"\\EOT", b"\\EOF") and body[pos + 4:].strip(b"\0") == b""):
			if is_hex:
				servers.append((socket.inet_ntoa(bytes.fromhex(body[pos + 1:pos + 9].decode())), int(body[pos + 9:pos + 13], 16)))
			else:
				servers.append((socket.inet_ntoa(body[pos + 1:pos + 5]), struct.unpack(">H", body[pos + 5:pos + 7])[0]))

			pos += size

	return servers


def query_masters(family, masters, args):
	packets_by_master = {}
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

	for host, port in masters:
		try:
			address = (socket.gethostbyname(host), port)

			for request in master_requests(family, args):
				sock.sendto(request, address)

			packets_by_master[address] = (host, [])
		except OSError as error:
			print(f"master {host}: {error}", file=sys.stderr)

	deadline = time.time() + MASTER_WAIT

	while time.time() < deadline:
		ready, _, _ = select.select([sock], [], [], max(0.0, deadline - time.time()))

		if ready:
			packet, source = sock.recvfrom(65535)

			if source in packets_by_master:
				packets_by_master[source][1].append(packet)

	return list(packets_by_master.values())


def query_status(family, servers):
	request = HEADER + (b"status\n" if family == "quake2" else b"getstatus")
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	pending = list(servers)
	sent = {}
	replies = {}

	while pending or sent:
		while pending and len(sent) < IN_FLIGHT:
			server = pending.pop()

			try:
				sock.sendto(request, server)
				sent[server] = time.time()
			except OSError as error:
				print(f"server {server[0]}:{server[1]}: {error}", file=sys.stderr)

		ready, _, _ = select.select([sock], [], [], 0.05)

		if ready:
			packet, source = sock.recvfrom(65535)

			# Some OpenJK servers ask for a challenge to be echoed before they answer.
			if source in sent and packet.startswith(HEADER + b'echo "echoResponse getstatus ') and packet.endswith(b'"'):
				sock.sendto(HEADER + packet[len(HEADER) + 6:-1], source)
				sent[source] = time.time()
			elif source in sent:
				del sent[source]
				replies[source] = packet

		for server, started in list(sent.items()):
			if time.time() - started > STATUS_WAIT:
				del sent[server]

	return replies


def describe(packet):
	lines = packet.decode("latin-1").split("\n")

	if len(lines) < 2:
		return "0/? ? (malformed reply)"

	fields = lines[1].split("\\")[1:]
	info = dict(zip((key.lower() for key in fields[::2]), fields[1::2]))
	players = [line for line in lines[2:] if line.strip()]
	name = next((info[key] for key in NAME_KEYS if key in info), "")
	maximum = next((info[key] for key in MAX_KEYS if key in info), "?")
	plain = re.sub(r"\^[^\^]", "", "".join(ch for ch in name if ord(ch) >= 32))
	return f"{len(players)}/{maximum} {info.get('mapname', '?')} {plain}"


def unreal2_string(text):
	return bytes([len(text) + 1]) + text.encode("ascii") + b"\0"


def unreal2_frame(payload):
	return struct.pack("<I", len(payload)) + payload


def unreal2_client_response():
	key = unreal2_string(UNREAL2_KEY_HASH)
	return unreal2_frame(key + key + unreal2_string("UT2K4CLIENT") + struct.pack("<IB", 3369, 0) + unreal2_string("int") + struct.pack("<IIIB", 0, 0, 0, 0))


def query_unreal2_master(host, port):
	"""Everything the master sent, and the query addresses it listed."""
	stream = bytearray()
	servers = []

	def read_frame(sock):
		size = struct.unpack("<I", read_exactly(sock, 4))[0]
		return read_exactly(sock, size)

	def read_exactly(sock, size):
		data = b""

		while len(data) < size:
			chunk = sock.recv(size - len(data))

			if not chunk:
				raise OSError("the master closed the connection early")

			data += chunk

		stream.extend(data)
		return data

	try:
		with socket.create_connection((host, port), timeout=MASTER_WAIT * 5) as sock:
			read_frame(sock)
			sock.sendall(unreal2_client_response())
			read_frame(sock)
			sock.sendall(unreal2_frame(unreal2_string(UNREAL2_KEY_HASH)))
			read_frame(sock)
			sock.sendall(unreal2_frame(b"\0\0"))
			count = struct.unpack("<I", read_frame(sock)[:4])[0]

			for _ in range(count):
				entry = read_frame(sock)
				servers.append((socket.inet_ntoa(entry[0:4]), struct.unpack("<H", entry[6:8])[0]))
	except OSError as error:
		print(f"master {host}: {error}", file=sys.stderr)

	return bytes(stream), servers


def query_unreal2_status(servers):
	"""Each server's packets, for those whose info packet came."""
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	pending = list(servers)
	last_heard = {}
	packets = {}

	while pending or last_heard:
		while pending and len(last_heard) < IN_FLIGHT:
			server = pending.pop()

			try:
				for query in UNREAL2_QUERIES:
					sock.sendto(query, server)

				last_heard[server] = time.time()
				packets[server] = []
			except OSError as error:
				print(f"server {server[0]}:{server[1]}: {error}", file=sys.stderr)

		ready, _, _ = select.select([sock], [], [], 0.05)

		if ready:
			packet, source = sock.recvfrom(65535)

			if source in last_heard:
				packets[source].append(packet)
				last_heard[source] = time.time()

		for server, heard in list(last_heard.items()):
			if time.time() - heard > (UNREAL2_QUIET if packets[server] else STATUS_WAIT):
				del last_heard[server]

	return {server: found for server, found in packets.items() if any(len(packet) > 4 and packet[4] == UNREAL2_INFO for packet in found)}


def read_unreal2_string(data, pos):
	"""A string whose length is a compact index: Latin-1 bytes when positive, UTF-16 units when negative."""
	first = data[pos]
	value, more, shift = first & 0x3F, first & 0x40, 6
	pos += 1

	while more:
		value |= (data[pos] & 0x7F) << shift
		more, shift = data[pos] & 0x80, shift + 7
		pos += 1

	length = -value if first & 0x80 else value

	if length < 0:
		return data[pos:pos - 2 * length].decode("utf-16-le", "replace").rstrip("\0"), pos - 2 * length

	return data[pos:pos + length].decode("latin-1").rstrip("\0"), pos + length


def describe_unreal2(packets):
	info = next(packet for packet in packets if len(packet) > 4 and packet[4] == UNREAL2_INFO)
	_, pos = read_unreal2_string(info, 9)
	name, pos = read_unreal2_string(info, pos + 8)
	map_name, pos = read_unreal2_string(info, pos)
	_, pos = read_unreal2_string(info, pos)
	players, maximum = struct.unpack("<ii", info[pos:pos + 8])
	plain_name, plain_map = (re.sub(r"\x1b...", "", text, flags=re.DOTALL) for text in (name, map_name))
	return f"{players}/{maximum} {plain_map} {''.join(ch for ch in plain_name if ord(ch) >= 32)}"


def main_unreal2(options, masters):
	answers = [(host, *query_unreal2_master(host, port)) for host, port in masters]
	servers = sorted({server for _, _, listed in answers for server in listed}, key=lambda server: (socket.inet_aton(server[0]), server[1]))
	replies = query_unreal2_status(servers)

	for server in servers:
		if server in replies:
			print(f"{server[0]}:{server[1]} {describe_unreal2(replies[server])}")

	print(f"{len(replies)} of {len(servers)} servers answered", file=sys.stderr)

	if options.save_fixtures is not None:
		target = options.save_fixtures / options.game
		target.mkdir(parents=True, exist_ok=True)

		for host, stream, _ in answers:
			(target / f"master-{host}-stream.bin").write_bytes(stream)

		chosen = sorted(replies.items(), key=lambda item: -int(describe_unreal2(item[1]).split("/")[0]))

		for (address, port), packets in chosen[:options.count]:
			for index, packet in enumerate(packets):
				(target / f"status-{address}_{port}-{index:02}.bin").write_bytes(packet)


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("game", choices=sorted(GAMES))
	parser.add_argument("--save-fixtures", type=pathlib.Path)
	parser.add_argument("--count", type=int, default=6, help="status replies to save")
	options = parser.parse_args()

	family, masters, args = GAMES[options.game]

	if family == "unreal2":
		main_unreal2(options, masters)
		return

	answers = query_masters(family, masters, args)
	servers = sorted({server for _, packets in answers for packet in packets for server in parse_master(family, packet, options.game in HEX_ENTRIES)},
		key=lambda server: (socket.inet_aton(server[0]), server[1]))
	replies = query_status(family, servers)

	for server in servers:
		if server in replies:
			print(f"{server[0]}:{server[1]} {describe(replies[server])}")

	print(f"{len(replies)} of {len(servers)} servers answered", file=sys.stderr)

	if options.save_fixtures is not None:
		target = options.save_fixtures / options.game
		target.mkdir(parents=True, exist_ok=True)

		for host, packets in answers:
			for index, packet in enumerate(packets):
				(target / f"master-{host}-{index}.bin").write_bytes(packet)

		chosen = sorted(replies.items(), key=lambda item: -int(describe(item[1]).split("/")[0]))

		for (address, port), packet in chosen[:options.count]:
			(target / f"status-{address}_{port}.bin").write_bytes(packet)


if __name__ == "__main__":
	main()
