#!/usr/bin/env python3
"""Unit tests for TC-4.2.3 (audit_licenses.py & generate_third_party_notices.py)."""

from __future__ import annotations

import pathlib
import sys
import tempfile
import unittest

SCRIPTS_DIR = pathlib.Path(__file__).resolve().parents[2] / "packaging" / "scripts"
sys.path.insert(0, str(SCRIPTS_DIR))

import audit_licenses  # noqa: E402
import generate_third_party_notices  # noqa: E402


class TestLicenseAudit(unittest.TestCase):
    """Verifies proprietary Adobe header detection and SPDX notice generation."""

    def test_clean_repo_passes(self) -> None:
        repo_root = pathlib.Path(__file__).resolve().parents[2]
        self.assertEqual(audit_licenses.main(["--root", str(repo_root)]), 0)

    def test_proprietary_header_detected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            leak = pathlib.Path(tmp) / "PrSDKImport.h"
            leak.write_text("/* ADOBE CONFIDENTIAL */", encoding="utf-8")
            self.assertEqual(audit_licenses.main(["--root", tmp]), 1)

    def test_notice_generation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            out_file = pathlib.Path(tmp) / "THIRD_PARTY_LICENSES.md"
            self.assertEqual(
                generate_third_party_notices.main(["--output", str(out_file)]), 0
            )
            content = out_file.read_text(encoding="utf-8")
            self.assertIn("LGPL-2.1-or-later", content)
            self.assertIn("MIT OR Apache-2.0", content)


if __name__ == "__main__":
    unittest.main()
