#!/usr/bin/env python3
"""Reference server query, independent of Lookout's C++ code.

    tools/query.py <game>                          list the game's servers, one line each
    tools/query.py <game> --save-fixtures <dir>    also save raw replies as parser test data

Each line is "<address> <players>/<max> <map> <name>", sorted by address. Compare `lookout --list <game>` with it
on the first three columns: names are only roughly decoded here.
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
}

HEADER = b"\xff\xff\xff\xff"
NAME_KEYS = ("hostname", "sv_hostname")
MAX_KEYS = ("maxclients", "sv_maxclients")
MASTER_WAIT = 2.0
STATUS_WAIT = 1.5
IN_FLIGHT = 32


def master_request(family, args):
	return b"query" if family == "quake2" else HEADER + b"getservers " + args.encode()


def parse_master(family, packet):
	servers = []

	if family == "quake2":
		if packet.startswith(HEADER + b"servers") and packet[len(HEADER) + 7:len(HEADER) + 8] in (b" ", b"\n"):
			body = packet[len(HEADER + b"servers") + 1:]
			servers = [(socket.inet_ntoa(body[i:i + 4]), struct.unpack(">H", body[i + 4:i + 6])[0]) for i in range(0, len(body) - 5, 6)]
	elif packet.startswith(HEADER + b"getserversResponse"):
		body = packet[len(HEADER + b"getserversResponse"):]
		pos = 0

		# "\EOT" ends a datagram only when nothing but padding follows: 69.79.84.x spells the same bytes.
		while pos + 7 <= len(body) and not (body[pos:pos + 4] == b"\\EOT" and body[pos + 4:].strip(b"\0") == b""):
			servers.append((socket.inet_ntoa(body[pos + 1:pos + 5]), struct.unpack(">H", body[pos + 5:pos + 7])[0]))
			pos += 7

	return servers


def query_masters(family, masters, args):
	packets_by_master = {}
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

	for host, port in masters:
		try:
			address = (socket.gethostbyname(host), port)
			sock.sendto(master_request(family, args), address)
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

			if source in sent:
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
	info = dict(zip(fields[::2], fields[1::2]))
	players = [line for line in lines[2:] if line.strip()]
	name = next((info[key] for key in NAME_KEYS if key in info), "")
	maximum = next((info[key] for key in MAX_KEYS if key in info), "?")
	plain = re.sub(r"\^[^\^]", "", "".join(ch for ch in name if ord(ch) >= 32))
	return f"{len(players)}/{maximum} {info.get('mapname', '?')} {plain}"


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("game", choices=sorted(GAMES))
	parser.add_argument("--save-fixtures", type=pathlib.Path)
	parser.add_argument("--count", type=int, default=6, help="status replies to save")
	options = parser.parse_args()

	family, masters, args = GAMES[options.game]
	answers = query_masters(family, masters, args)
	servers = sorted({server for _, packets in answers for packet in packets for server in parse_master(family, packet)},
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
