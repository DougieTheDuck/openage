# Portable Linux package

A self-contained `.tar.gz`, the Linux counterpart of the Windows NSIS/7z
packages. Extract it anywhere and start the game with `./run.sh`.

It bundles Python, the Python dependencies, Qt (with its plugins and QML
modules), nyan and every other library the game links against. It relies on
the system for the same things every Linux game does: glibc, libstdc++, GPU
drivers (OpenGL/Vulkan), X11/Wayland, PulseAudio/PipeWire, fontconfig and D-Bus.

## Building

The interpreter you build with gets copied into the package, so it has to be
relocatable. The distro's `/usr/bin/python3` is refused; use a standalone
build instead:

```sh
uv python install 3.12
PY=$(uv python find 3.12)
$PY -m pip install cython lz4 mako numpy pillow pygments setuptools tomli-w

cmake -S . -B build-linux -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLINUX_BUNDLE=ON \
    -DDOWNLOAD_NYAN=YES \
    -DPython3_EXECUTABLE=$PY
cmake --build build-linux
cd build-linux && cpack -G TGZ
```

The result is `openage-<version>-Linux-portable.tar.gz` in `build-linux`.
Besides the usual build dependencies, Qt's QML modules must be installed
(on Debian/Ubuntu: `qml6-module-qtqml`, `qml6-module-qtqml-models`,
`qml6-module-qtqml-workerscript`, `qml6-module-qtquick-controls`).

## Which distros it runs on

The package needs a glibc and libstdc++ at least as new as the machine it was
built on. Build on the oldest distro you want to support: a build on Ubuntu
24.04 runs on Ubuntu 24.04+, Debian 13+, Fedora 40+, Arch and similar.

## How it works

* `LINUX_BUNDLE=ON` makes the asset and config dirs relative and installs the
  openage Python package into the bundled interpreter's site-packages.
* At CPack time `buildsystem/scripts/bundle_linux.py` copies the interpreter,
  the Qt plugins and QML modules (including their `qmldir` dependencies), and
  every non-system library found with `ldd`. The list of libraries left to the
  system is `EXCLUDE_PREFIXES` in that script.
* `run.sh` points `LD_LIBRARY_PATH` and Qt at the bundled copies, changes to
  the package directory (like `openage.bat`) and runs `openage game`.
* On a Wayland session `run.sh` uses XWayland (`QT_QPA_PLATFORM=xcb;wayland`),
  falling back to native Wayland if XWayland is missing. Set `QT_QPA_PLATFORM`
  yourself to override, e.g. `QT_QPA_PLATFORM=wayland ./run.sh`.
