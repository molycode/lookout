Lookout
=======

A game server browser for Linux. It lists the servers from each game's master servers,
filters and sorts them, shows who is playing, and joins with a double-click through the
game's own launcher.

Lookout knows no game by itself. Each game is a description and each query protocol a
sandboxed Lua script, downloaded from github.com/molycode/lookout-games or written by you,
so adding a game, even one still in development, needs no new Lookout.

Requirements
------------
x86-64 (64-bit PC) Linux with glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36 and later),
on an X11 or Wayland desktop with OpenGL.

Install
-------
    ./install.sh

This installs Lookout for your user only, under ~/.local, and adds it to the applications
menu. It needs no root. To upgrade, unpack a newer package and run its install.sh.

Games
-----
Lookout comes without games. On its first start it offers to download them from
github.com/molycode/lookout-games; later, the download button above the game list installs,
updates and removes them. From a terminal, the same is

    lookout --download [game...]

which downloads the games named, or every game missing or with an update. They are kept in
~/.local/share/lookout/downloaded/.

Starting a game
---------------
Lookout finds most installs by itself. When it says it cannot start a game, it found no
way to: open the gear beside the game in the sidebar and add your install.

Adding and changing games
-------------------------
Click the pencil on a game in the sidebar to edit its description, or the gamepad above
the game list to add a game. The editor offers each field with what it means, checks it as
you type, and can also revert a downloaded game to as downloaded or remove a game of your
own. It keeps a description in

    ~/.local/share/lookout/games/<name>/game.json

where a downloaded game's file holds only your changes, so updates of the game keep them.
Protocol scripts of your own go in ~/.local/share/lookout/protocols/; one named like a
downloaded protocol replaces it, and a game's settings (the gear) show which one is in use
and the downloaded one's version. Lookout reloads both folders when they change. To publish
a game, see github.com/molycode/lookout-games.

Uninstall
---------
    ./uninstall.sh

or, once this folder is gone, the copy install.sh keeps:

    sh ~/.local/share/lookout/uninstall.sh

Either removes the program, its menu entry and its icon. Your settings, logs and games
stay:

    Settings  ~/.config/lookout/
    Logs      ~/.local/state/lookout/logs/
    Games     ~/.local/share/lookout/ (downloaded/, and your own games/ and protocols/)

These paths, and that of the uninstaller's copy, follow $XDG_CONFIG_HOME, $XDG_STATE_HOME
and $XDG_DATA_HOME when those are set.

Licence
-------
MIT, see LICENSE. The notices of the libraries and fonts Lookout includes are in the
program, under Lookout > About Lookout > Licences. The games' icons are not Lookout's: each
comes with an icon-licence.txt giving its source and licence.
