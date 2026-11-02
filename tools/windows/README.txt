Lookout
=======

A game server browser for Windows and Linux. It lists the servers from each game's master
servers, filters and sorts them, shows who is playing, and joins with a double-click through
the game's own launcher.

Lookout knows no game by itself. Each game is a description and each query protocol a
sandboxed Lua script, downloaded from github.com/molycode/lookout-games or written by you,
so adding a game, even one still in development, needs no new Lookout.

Requirements
------------
64-bit Windows 10 22H2 or Windows 11; tested on Windows 11.

Install
-------
Run lookout-<version>-x86_64-setup.exe. It installs Lookout for you alone, in
%LOCALAPPDATA%\Programs\Lookout, with a Start menu entry, and needs no administrator rights.
To upgrade, run a newer setup: it installs where Lookout already is.

Unless you untick it, Setup adds that folder to your PATH, so that in cmd and PowerShell

    lookout --help

runs Lookout's command line. Git Bash runs lookout.exe for "lookout"; type lookout.com there.

Games
-----
Lookout comes without games. On its first start it offers to download them from
github.com/molycode/lookout-games; later, the download button above the game list installs,
updates and removes them. From a terminal, the same is

    lookout --download [game...]

which downloads the games named, or every game missing or with an update. They are kept in
%APPDATA%\Lookout\downloaded\. When games need a newer Lookout, Download games names it and
links to its release. The icon at the right of the menu bar says whether a newer Lookout is
out; Lookout asks GitHub at start and once a day. Click the icon to turn that off, or untick
"Check for new versions" in Setup.

Starting a game
---------------
Lookout finds most installs by itself, through Steam, GOG and the usual folders. When it says
it cannot start a game, it found no way to: open the gear beside the game in the sidebar and
add your install.

Adding and changing games
-------------------------
Click the pencil on a game in the sidebar to edit its description, or the gamepad above
the game list to add a game. The editor offers each field with what it means, checks it as
you type, and can also revert a downloaded game to as downloaded or remove a game of your
own. It keeps a description in

    %APPDATA%\Lookout\games\<name>\game.json

where a downloaded game's file holds only your changes, so updates of the game keep them.
The editor tests each master as you open and edit it: a check with the number of servers
it listed, or a cross with the reason. Protocol scripts of your own go in
%APPDATA%\Lookout\protocols\; one named like a downloaded protocol replaces it, and a
game's settings (the gear) show which one is in use and the downloaded one's version.
Lookout reloads both folders when they change. To publish a game, see
github.com/molycode/lookout-games.

Uninstall
---------
In Settings > Apps > Installed apps, choose Lookout, then Uninstall. This removes the
program, its shortcuts, its PATH entry and the game icons it keeps in
%LOCALAPPDATA%\Lookout\icons\. Your settings, logs and games stay:

    Settings  %APPDATA%\Lookout\config.json
    Logs      %LOCALAPPDATA%\Lookout\logs\
    Games     %APPDATA%\Lookout\ (downloaded\, and your own games\ and protocols\)

To remove those too, tick "Settings, logs and games" in the uninstaller.

These folders follow XDG_CONFIG_HOME, XDG_DATA_HOME, XDG_STATE_HOME and XDG_CACHE_HOME
when those are set to absolute paths, and the uninstaller follows them too.

Licence
-------
MIT, see LICENSE.txt. The notices of the libraries and fonts Lookout includes are in the
program, under Lookout > About Lookout > Licences. The games' icons are not Lookout's: each
comes with an icon-licence.txt giving its source and licence.
