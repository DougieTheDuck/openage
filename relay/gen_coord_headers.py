# Copyright 2026 the openage authors. See copying.md for legal info.

"""
Generate libopenage's coord_*.gen.h/.cpp for the standalone relay build.

The relay includes network/messages.h, which includes the generated
coordinate types. This runs openage's own coord generator
(openage/codegen/coord.py) on its own, without importing the rest of the
openage Python package, and writes the output under a build directory.

usage: gen_coord_headers.py <openage source root> <output dir>
"""

import importlib.util
import pathlib
import sys


class SplitProjectDir:
    """
    Stand-in for the util.fslike.path.Path the generator expects: templates
    are read from the source tree, generated files go to the output dir.
    """

    def __init__(self, source: pathlib.Path, output: pathlib.Path, parts=()):
        self.source = source
        self.output = output
        self.parts = parts

    def joinpath(self, name):
        return SplitProjectDir(self.source, self.output, self.parts + (name,))

    def open(self, mode="r"):
        if "w" in mode:
            path = self.output.joinpath(*self.parts)
            path.parent.mkdir(parents=True, exist_ok=True)
            return path.open(mode)
        return self.source.joinpath(*self.parts).open(mode)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    source = pathlib.Path(sys.argv[1]).resolve()
    output = pathlib.Path(sys.argv[2]).resolve()

    spec = importlib.util.spec_from_file_location(
        "openage_codegen_coord", source / "openage" / "codegen" / "coord.py")
    coord = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(coord)
    coord.generate_coord_basetypes(SplitProjectDir(source, output))


if __name__ == "__main__":
    main()
