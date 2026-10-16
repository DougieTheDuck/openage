# Copyright 2026 the openage authors. See copying.md for legal info.

"""Regression tests for code-compliance behavior on Windows checkouts."""

import importlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from . import headerguards, legal, textfiles
from .util import normalize_path, readfile


class WindowsCheckoutTest(unittest.TestCase):
    """Verify that checkout-specific paths and line endings do not affect checks."""

    def test_readfile_normalizes_line_endings(self):
        with tempfile.TemporaryDirectory() as tmpdirname:
            filename = Path(tmpdirname) / "line_endings.txt"
            filename.write_bytes(b"first\r\nsecond\rthird\n")

            self.assertEqual(readfile(filename), "first\nsecond\nthird\n")

    def test_changed_files_use_repository_separators(self):
        compliance = importlib.import_module("buildsystem.codecompliance.__main__")

        with patch.object(compliance.subprocess, "check_output", return_value=b"dir\\file.h\0"):
            changed_files = compliance.get_changed_files("HEAD")

        self.assertEqual(changed_files, {"dir/file.h"})

    def test_checks_accept_crlf_and_only_inspect_changed_files(self):
        with tempfile.TemporaryDirectory() as tmpdirname:
            root = Path(tmpdirname)
            valid_header = root / "valid.h"
            invalid_header = root / "invalid.h"
            valid_source = root / "valid.py"
            invalid_source = root / "invalid.py"

            valid_header.write_bytes(
                b"// Copyright 2026 the openage authors. See copying.md for legal info.\r\n"
                b"\r\n#pragma once\r\n"
            )
            invalid_header.write_bytes(b"invalid\r\n")
            valid_source.write_bytes(
                b"# Copyright 2026 the openage authors. See copying.md for legal info.\r\n\r\nvalue = 1\r\n"
            )
            invalid_source.write_bytes(b"invalid \r\n")

            changed_files = {
                normalize_path(valid_header),
                normalize_path(valid_source),
            }

            self.assertEqual(list(headerguards.find_issues(tmpdirname, changed_files)), [])
            self.assertEqual(list(legal.find_issues((tmpdirname,), changed_files)), [])
            self.assertEqual(list(textfiles.find_issues((tmpdirname,), (".h", ".py"), changed_files)), [])


if __name__ == "__main__":
    unittest.main()
