# Copyright 2026 the openage authors. See copying.md for legal info.

"""
Turn a LINUX_BUNDLE install tree into a self-contained, relocatable package.

Called from EmbedLinuxDependencies.cmake at install/CPack time, using the
interpreter openage was built with. That interpreter is copied to python/,
so it has to be a relocatable build such as python-build-standalone
(`uv python install 3.12`), not the distro's /usr/bin/python3.

Afterwards the tree contains:
    run.sh                  launcher (installed separately by CMake)
    python/                 interpreter + openage + Python dependencies
    <libdir>/               libopenage, nyan and every non-system shared lib
    <libdir>/qt6/plugins    Qt platform/image/multimedia plugins
    <libdir>/qt6/qml        QML modules the game's UI imports
    share/openage           game assets
"""

import argparse
import os
import shutil
import subprocess
import sys
import sysconfig
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from deploy_qt import copy_qml_modules  # pylint: disable=wrong-import-position


# Libraries taken from the host system rather than bundled: the C runtime and
# anything tied to the user's GPU driver, display server or audio/font setup.
# Bundling these breaks on other machines (mismatched drivers, or symbol
# clashes when Mesa loads its own libstdc++). Matched by soname prefix.
EXCLUDE_PREFIXES = (
    # glibc
    "ld-linux", "libc.so", "libm.so", "libdl.so", "libpthread.so", "librt.so",
    "libresolv.so", "libutil.so", "libmvec.so", "libanl.so", "libBrokenLocale.so",
    "libnss_", "libthread_db.so",
    # compiler runtime (needed by GPU drivers at the host's version)
    "libstdc++.so", "libgcc_s.so",
    # graphics drivers
    "libGL.so", "libGLX", "libGLdispatch", "libOpenGL", "libEGL", "libGLES",
    "libglapi", "libvulkan", "libdrm", "libgbm",
    # display server core
    "libX11.so", "libX11-xcb", "libxcb.so", "libxcb-dri", "libxcb-glx",
    "libxcb-present", "libxcb-sync", "libxshmfence", "libXau", "libXdmcp",
    "libwayland-client", "libwayland-egl", "libwayland-server",
    # audio, fonts, desktop services
    "libasound", "libpulse", "libjack", "libpipewire",
    "libfontconfig", "libfreetype", "libexpat",
    "libdbus-1", "libudev", "libsystemd",
)

# Qt plugin folders the game uses. The host's plugin dir can also hold plugins
# from every installed Qt app (on Arch, all of KDE Frameworks), so only these
# are copied.
KEEP_QT_PLUGINS = {
    # windowing on X11 and Wayland
    "platforms", "platforminputcontexts", "xcbglintegrations",
    "wayland-decoration-client", "wayland-graphics-integration-client",
    "wayland-shell-integration",
    # images, icons, sound, networking
    "imageformats", "iconengines", "multimedia", "tls", "networkinformation",
}

# Parts of the interpreter only needed for building, not running.
SKIP_PYTHON = {"include", "share", "__pycache__", "test", "idlelib", "tkinter",
               "turtledemo", "Cython", "pyximport", "pkgconfig"}


def is_excluded(soname):
    return soname.startswith(EXCLUDE_PREFIXES)


def is_elf(path):
    try:
        with open(path, "rb") as handle:
            return handle.read(4) == b"\x7fELF"
    except OSError:
        return False


def copy_python(prefix):
    """Copy the build interpreter and its site-packages into python/."""
    source = Path(sys.prefix).resolve()
    if str(source).startswith("/usr"):
        sys.exit(
            f"bundle_linux: {sys.executable} is the system Python ({source}), which isn't "
            "relocatable. Configure with a standalone build, e.g. "
            "`uv python install 3.12` and -DPython3_EXECUTABLE=<that python>."
        )

    destination = prefix / "python"

    def ignore(directory, names):
        skipped = {name for name in names if name in SKIP_PYTHON or name.lower().startswith("cython")}
        skipped |= {name for name in names if name.endswith(".a")}
        # tcl/tk runtime, only used by tkinter
        if Path(directory).name == "lib":
            skipped |= {name for name in names if name.startswith(("tcl", "tk", "itcl", "thread"))
                        and Path(directory, name).is_dir()}
        return skipped

    # dirs_exist_ok: openage's own modules were already installed into
    # python/lib/pythonX.Y/site-packages by the regular install step.
    shutil.copytree(source, destination, symlinks=True, ignore=ignore, dirs_exist_ok=True)
    print(f"bundle_linux: copied Python {sys.version.split()[0]} from {source}")


def qml_dependencies(modules, qml):
    """
    qmlimportscanner only reports the UI's direct imports. Modules pull in
    others through `depends`/`import` lines in their qmldir (QtQuick needs
    QtQml.WorkerScript, for example), so follow those too.
    """
    seen = {module["name"] for module in modules if "name" in module}
    queue = [Path(module["path"]) for module in modules if "path" in module]
    extra = []
    while queue:
        qmldir = queue.pop() / "qmldir"
        if not qmldir.is_file():
            continue
        for line in qmldir.read_text(errors="replace").splitlines():
            words = line.split()
            if len(words) < 2 or words[0] not in ("depends", "import", "optional"):
                continue
            name = words[2] if words[0] == "optional" and len(words) > 2 else words[1]
            if words[0] == "optional" and words[1] != "import":
                continue
            name = name.split("/")[0]
            path = qml.joinpath(*name.split("."))
            if name in seen or not (path / "qmldir").is_file():
                continue
            seen.add(name)
            relative = path.relative_to(qml).as_posix()
            extra.append({"name": name, "path": str(path), "relativePath": relative})
            queue.append(path)
    return extra


def copy_qt(prefix, libdir, asset_dir, plugins, qml, scanner):
    """Copy Qt plugins, the QML modules the UI imports, and a qt.conf."""
    qt_dir = prefix / libdir / "qt6"

    for entry in sorted(plugins.iterdir()):
        if entry.is_dir() and entry.name in KEEP_QT_PLUGINS:
            shutil.copytree(entry, qt_dir / "plugins" / entry.name, dirs_exist_ok=True)

    scan = subprocess.run(
        [str(scanner), "-rootPath", str(prefix / asset_dir), "-importPath", str(qml)],
        check=True, capture_output=True, text=True,
    )
    import json  # pylint: disable=import-outside-toplevel
    modules = json.loads(scan.stdout)
    copy_qml_modules(modules + qml_dependencies(modules, qml), qt_dir / "qml")

    # Qt reads qt.conf next to the executable (python/bin). Without it the
    # bundled QtCore would also search the host's Qt plugin dirs, and loading
    # plugins built for a different Qt version crashes.
    rel = os.path.relpath(qt_dir, prefix / "python" / "bin")
    (prefix / "python" / "bin" / "qt.conf").write_text(
        "[Paths]\n"
        f"Prefix = {rel}\n"
        "Plugins = plugins\n"
        "QmlImports = qml\n"
        "Qml2Imports = qml\n"
    )
    print(f"bundle_linux: copied Qt plugins and QML modules from {plugins.parent}")


def copy_shared_libs(prefix, libdir):
    """Copy every non-system library the bundled ELF files link against."""
    lib_dest = prefix / libdir
    lib_dest.mkdir(parents=True, exist_ok=True)

    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = os.pathsep.join(
        [str(lib_dest), str(prefix / "python" / "lib")] + env.get("LD_LIBRARY_PATH", "").split(os.pathsep)
    )

    elfs = [path for path in prefix.rglob("*")
            if path.is_file() and not path.is_symlink() and is_elf(path)]

    copied, missing = {}, set()
    for elf in elfs:
        result = subprocess.run(["ldd", str(elf)], capture_output=True, text=True, env=env, check=False)
        for line in result.stdout.splitlines():
            if "=>" not in line:
                continue
            soname, target = (part.strip() for part in line.split("=>", 1))
            # some distros (e.g. Arch) list the dynamic loader by full path,
            # "/lib64/ld-linux-x86-64.so.2 => ...", so only keep the file name
            soname = Path(soname).name
            if is_excluded(soname):
                continue
            if target.startswith("not found"):
                missing.add((soname, elf.relative_to(prefix)))
                continue
            path = Path(target.split(" (")[0]).resolve()
            if path.is_relative_to(prefix) or soname in copied:
                continue
            destination = lib_dest / soname
            if destination.parent != lib_dest:
                sys.exit(f"bundle_linux: refusing to copy {path} outside the package ({destination})")
            shutil.copy2(path, destination)
            copied[soname] = path

    for soname, path in sorted(copied.items()):
        print(f"bundle_linux: bundled {soname} <- {path}")

    # Optional Qt plugins (e.g. eglfs backends) may legitimately miss deps;
    # anything else missing means the package will not start.
    fatal = sorted((soname, elf) for soname, elf in missing
                   if soname not in copied and "plugins" not in elf.parts)
    for soname, elf in sorted(missing):
        if soname not in copied:
            print(f"bundle_linux: warning: {elf} needs {soname}, which was not found")
    if fatal:
        sys.exit("bundle_linux: required libraries are missing, see warnings above")


def main():
    cli = argparse.ArgumentParser()
    cli.add_argument("--prefix", type=Path, required=True)
    cli.add_argument("--libdir", required=True)
    cli.add_argument("--asset-dir", required=True)
    cli.add_argument("--qt-plugins", type=Path, required=True)
    cli.add_argument("--qt-qml", type=Path, required=True)
    cli.add_argument("--qmlimportscanner", type=Path, required=True)
    args = cli.parse_args()

    prefix = args.prefix.resolve()

    # development files installed by nyan and friends aren't needed to play
    for dev_dir in (prefix / "include", prefix / args.libdir / "cmake", prefix / args.libdir / "pkgconfig"):
        shutil.rmtree(dev_dir, ignore_errors=True)

    copy_python(prefix)
    copy_qt(prefix, args.libdir, args.asset_dir, args.qt_plugins, args.qt_qml, args.qmlimportscanner)
    copy_shared_libs(prefix, args.libdir)
    print(f"bundle_linux: done ({sysconfig.get_platform()})")


if __name__ == "__main__":
    main()
