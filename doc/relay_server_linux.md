# Headless relay server (Linux)

The relay is the server players connect to for multiplayer: hosts advertise a
room on it and guests join through it. `openage relay` runs it from a full game
install; this is a separate, small build of just the relay for a headless
server. It has no Python, Qt, graphics or audio, and the binary only needs the
system's glibc.

## Building

You need a C++20 compiler (tested with GCC 13), CMake, and Python 3 with the
`mako` module. Python is only used at build time, to generate the coordinate
headers the relay's network code includes; the relay itself doesn't use it.

```sh
sudo apt install build-essential cmake python3-mako    # Debian/Ubuntu
sudo pacman -S --needed base-devel cmake python-mako   # Arch
```

Then, from the repository root:

```sh
cmake -S relay -B build-relay -DCMAKE_BUILD_TYPE=Release
cmake --build build-relay
```

The binary is `build-relay/openage-relay`. To make a tarball to copy to a server:

```sh
cd build-relay && cpack
```

This produces `openage-relay-v<version>-Linux-x86_64.tar.gz` (the binary, this
file and an example systemd unit).

Like any Linux binary, it runs on systems with a glibc at least as new as the
build machine's, so build on the oldest distro you deploy to (or the same one).

## Running

```sh
./openage-relay                              # public access on TCP port 32456
./openage-relay --port 40000
./openage-relay --player-token <64 hex chars> --player-token <...>   # private alpha
./openage-relay --help
```

The options match `openage relay`. Stop it with Ctrl+C or SIGTERM. Players enter
the server's address and port in the lobby, so open the TCP port in the
server's firewall.

## Running as a service

`openage-relay.service` in the package is an example systemd unit, set up for
the package extracted to `/opt/openage-relay`:

```sh
sudo mkdir -p /opt/openage-relay
sudo tar xzf openage-relay-*.tar.gz -C /opt/openage-relay --strip-components=1
sudo cp /opt/openage-relay/openage-relay.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now openage-relay
journalctl -u openage-relay -f
```

Edit `ExecStart` in the unit to change the port or add player tokens.

## Keeping it in sync

`relay/CMakeLists.txt` lists the libopenage files the relay is built from. If a
change to the relay code uses another file, the link step fails with an
undefined symbol; add that file's `.cpp` to `RELAY_LIBOPENAGE_SOURCES`.
