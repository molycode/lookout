#!/usr/bin/env python3
"""Renders Lookout's icon, data/lookout.svg, into data/windows/lookout.ico for the Windows programs.

    tools/make-ico.py

Needs rsvg-convert, or else Edge, Chrome or Chromium, run headless. Each size of Microsoft's Win32 set is drawn from the
SVG, never scaled from another size, and stored as a PNG frame.
"""

import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SVG = ROOT / "data" / "lookout.svg"
ICO = ROOT / "data" / "windows" / "lookout.ico"

# 36 is the taskbar at 150 %; 256 is Explorer's largest view.
SIZES = (16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 256)

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
RGBA = 6
BROWSER_ATTEMPTS = 3
BROWSER_PATIENCE = 20

PAGE = """<!doctype html>
<html><head><style>html, body {{ margin: 0; background: transparent; }} img {{ display: block; }}</style></head>
<body><img src="{svg}" width="{size}" height="{size}"></body></html>
"""


def find_browser():
	names = ("msedge.exe", "chrome.exe") if sys.platform == "win32" else ("microsoft-edge", "google-chrome", "chromium", "chromium-browser")
	found = None

	for name in names:
		if found is None and sys.platform == "win32":
			found = find_registered_program(name)

		if found is None:
			found = shutil.which(name)

	return found


def find_registered_program(name):
	import winreg

	found = None

	for root in (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE):
		try:
			with winreg.OpenKey(root, rf"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\{name}") as key:
				path = winreg.QueryValue(key, None)

				if found is None and os.path.isfile(path):
					found = path
		except OSError:
			pass

	return found


def render_with_rsvg(rsvg, folder):
	frames = {}

	for size in SIZES:
		path = folder / f"{size}.png"
		subprocess.run([rsvg, "-w", str(size), "-h", str(size), "-o", str(path), str(SVG)], check=True)
		frames[size] = path.read_bytes()

	return frames


def render_with_browser(browser, folder):
	frames = {}

	for size in SIZES:
		page = folder / f"{size}.html"
		path = folder / f"{size}.png"
		page.write_text(PAGE.format(svg=SVG.as_uri(), size=size))

		# Headless Edge now and then never takes its screenshot, so a stuck attempt is ended and another started afresh.
		for attempt in range(BROWSER_ATTEMPTS):
			if not path.is_file():
				take_screenshot(browser, page, path, size, folder / f"profile-{size}-{attempt}")

		if path.is_file():
			frames[size] = path.read_bytes()

	return frames


def take_screenshot(browser, page, path, size, profile):
	# A profile of its own, so the user's browser is neither read nor changed; one pixel per CSS pixel, whatever the
	# display's scale. Started from an administrator's prompt, Edge and Chrome would otherwise hand the work to a copy
	# without the rights, which can neither open the private temporary folder nor be waited for.
	try:
		subprocess.run([browser, "--headless", "--disable-gpu", "--no-first-run", "--do-not-de-elevate", "--hide-scrollbars",
			"--force-color-profile=srgb", "--force-device-scale-factor=1", "--default-background-color=00000000",
			f"--user-data-dir={profile}", f"--window-size={size},{size}", f"--screenshot={path}", page.as_uri()],
			stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=BROWSER_PATIENCE)
	except subprocess.TimeoutExpired:
		path.unlink(missing_ok=True)


def read_frame(png):
	"""Its width, height and first pixel's alpha (None unless 8-bit RGBA; no PNG filter changes that pixel), or None
	for what is not a whole PNG."""

	frame = None

	try:
		width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", png[16:29])
		data = b""
		offset = len(PNG_SIGNATURE)

		while offset < len(png):
			length, kind = struct.unpack(">I4s", png[offset:offset + 8])

			if kind == b"IDAT":
				data += png[offset + 8:offset + 8 + length]

			offset += 12 + length

		pixels = zlib.decompress(data)
		corner = pixels[4] if depth == 8 and colour == RGBA and interlace == 0 else None
		frame = (width, height, corner) if png.startswith(PNG_SIGNATURE) else None
	except (struct.error, zlib.error, IndexError):
		pass

	return frame


def check_frames(frames):
	problems = []

	for size in SIZES:
		frame = read_frame(frames[size]) if size in frames else None

		if size not in frames:
			problems.append(f"no {size} px frame was drawn")
		elif frame is None:
			problems.append(f"the {size} px frame is not a whole PNG")
		elif frame[:2] != (size, size):
			problems.append(f"the {size} px frame is {frame[0]}x{frame[1]}")
		elif frame[2] is None:
			problems.append(f"the {size} px frame is not 8-bit RGBA")
		elif frame[2] != 0:
			problems.append(f"the {size} px frame has no transparent corner")

	return problems


def write_ico(frames):
	directory = struct.pack("<HHH", 0, 1, len(SIZES))
	offset = len(directory) + 16 * len(SIZES)
	entries = b""
	images = b""

	for size in SIZES:
		png = frames[size]
		# The entry's byte holds 0 for 256.
		entries += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(png), offset + len(images))
		images += png

	ICO.parent.mkdir(parents=True, exist_ok=True)
	ICO.write_bytes(directory + entries + images)


def main():
	rsvg = shutil.which("rsvg-convert")
	browser = None if rsvg else find_browser()
	exit_code = 1

	if rsvg is None and browser is None:
		print("make-ico: needs rsvg-convert, or Edge, Chrome or Chromium", file=sys.stderr)
	else:
		# A browser ended for being stuck may still hold its profile for a moment.
		with tempfile.TemporaryDirectory(prefix="lookout-ico-", ignore_cleanup_errors=True) as temporary:
			folder = pathlib.Path(temporary)
			frames = render_with_rsvg(rsvg, folder) if rsvg else render_with_browser(browser, folder)

		problems = check_frames(frames)

		for problem in problems:
			print(f"make-ico: {problem}", file=sys.stderr)

		if not problems:
			write_ico(frames)
			print(f"{ICO.relative_to(ROOT)}: {len(SIZES)} frames from {rsvg or browser}")
			exit_code = 0

	return exit_code


if __name__ == "__main__":
	sys.exit(main())
