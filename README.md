# Lookout

A native Linux game server browser. It lists the servers from each game's masters, filters and sorts them, shows
who is playing, and joins with a double-click through the game's own launcher.

![The Lookout window: the games in the sidebar, their servers in the list, and the selected server's players and rules](docs/screenshot.png)

## Building

Needs CMake 4.3 or newer, Ninja, and GCC 14+ or Clang 19+.

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
./build/gcc-Release/lookout
```

The first configure fetches the submodules. `tools/build-release.sh` builds the distributable binary in a
container, so that it runs on Ubuntu 22.04, Debian 12 and anything newer. `tools/make-package.sh` then wraps it into
`dist/lookout-<version>-x86_64.tar.xz`, with a menu entry, an icon and an `install.sh` that installs to `~/.local`.

## License

MIT — see `LICENSE`.
