Free Direct server for Windows (64-bit)
=======================================

Internet multiplayer for the Free API builds of Speedy Blupi and Speedy Eggbert
(and other games built with Free Direct). Every player, the host included,
connects out to this server; the players need no open ports.

Starting
--------
Double-click start-server.bat. A console window shows the server; close it (or
press Ctrl+C) to stop the server. It listens on TCP port 51325.

* Windows Firewall asks on the first start: allow it. For players outside your
  network, forward TCP port 51325 on your router to this computer, and give the
  players your public IP address (or a host name).
* A password: write it on the first line of password.txt in this folder before
  starting. start-server.bat then passes it with --password-file.
* Other options: free-direct-server.exe --help, for example --port 51400.
  Extra arguments of start-server.bat are passed on to the server.

Playing
-------
1. In the game's multiplayer screen, choose the provider "FreeDirect server".
2. Enter the server's address (this computer's IP address, or "localhost" on
   this computer), port 51325 and the password if there is one.
3. One player creates a room and gives its ID to the others, who join the room
   with that ID.
4. Inside the room, create a game or join one, as on a local network.

Only the same game, edition and language play together in a room.

This build serves plain ws:// (no TLS). Native games connect to it directly.
Browser games on an https:// page (GitHub Pages) need wss://, which this build
cannot serve; put a TLS reverse proxy in front of it for them.

Licences: the folder licenses (Free Direct, IXWebSocket, the MinGW-w64 runtime).
