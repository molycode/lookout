Lookout
=======

A game server browser for Linux. It lists the servers from each game's master servers,
filters and sorts them, shows who is playing, and joins with a double-click through the
game's own launcher.

Requirements
------------
x86-64 (64-bit PC) Linux with glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36 and later),
on an X11 or Wayland desktop with OpenGL.

Install
-------
    ./install.sh

This installs Lookout for your user only, under ~/.local, and adds it to the applications
menu. It needs no root. To upgrade, unpack a newer package and run its install.sh.

Starting a game
---------------
Lookout finds most installs by itself. When it says it cannot start a game, it found no
way to: open the gear beside the game in the sidebar and add your install.

Adding and changing games
-------------------------
Right-click a game in the sidebar to edit its description, or click the gamepad beside the
"+" above the server list to add a game. Lookout checks a description as you type and
keeps it in

    ~/.local/share/lookout/games/<name>/game.json

where a built-in game's file holds only your changes. Protocol scripts of your own go in
~/.local/share/lookout/protocols/. Lookout reloads both folders when they change.

Uninstall
---------
    ./uninstall.sh

or, once this folder is gone, the copy install.sh keeps:

    sh ~/.local/share/lookout/uninstall.sh

Either removes the program, its menu entry and its icon. Your settings, logs and games
stay:

    Settings  ~/.config/lookout/
    Logs      ~/.local/state/lookout/logs/
    Games     ~/.local/share/lookout/games/ and protocols/

These paths, and that of the uninstaller's copy, follow $XDG_CONFIG_HOME, $XDG_STATE_HOME
and $XDG_DATA_HOME when those are set.

Licence
-------
MIT, see LICENSE. The notices of the libraries and fonts Lookout includes are in the
program, under Lookout > About Lookout > Licences. The games' icons are not Lookout's: each
comes with an icon-licence.txt giving its source and licence.
