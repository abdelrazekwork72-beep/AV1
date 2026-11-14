#!/usr/bin/env python3
"""Unit tests for TC-4.2.3 (audit_licenses.py, generate_third_party_notices.py, and packaging configs)."""

from __future__ import annotations

import os
import pathlib
import stat
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "packaging" / "scripts"
sys.path.insert(0, str(SCRIPTS_DIR))

import audit_licenses  # noqa: E402
import generate_third_party_notices  # noqa: E402


class TestLicenseAuditAndPackaging(unittest.TestCase):
    """Verifies proprietary Adobe header detection, SPDX boundary enforcement, and installer configs."""

    def test_clean_repo_passes(self) -> None:
        self.assertEqual(audit_licenses.main(["--root", str(REPO_ROOT)]), 0)

    def test_proprietary_header_detected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            leak = pathlib.Path(tmp) / "PrSDKImport.h"
            leak.write_text("/* ADOBE CONFIDENTIAL */", encoding="utf-8")
            self.assertEqual(audit_licenses.main(["--root", tmp]), 1)

    def test_connector_ffmpeg_boundary_violation_detected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            imp_dir = pathlib.Path(tmp) / "src" / "importer"
            imp_dir.mkdir(parents=True, exist_ok=True)
            bad_cpp = imp_dir / "leak.cpp"
            bad_cpp.write_text("#include <libavcodec/avcodec.h>\n", encoding="utf-8")
            self.assertEqual(audit_licenses.main(["--root", tmp]), 1)

    def test_notice_generation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            out_file = pathlib.Path(tmp) / "THIRD_PARTY_LICENSES.md"
            self.assertEqual(
                generate_third_party_notices.main(["--output", str(out_file)]), 0
            )
            content = out_file.read_text(encoding="utf-8")
            self.assertIn("LGPL-2.1-or-later", content)
            self.assertIn("GPL-3.0-or-later", content)
            self.assertIn("MIT OR Apache-2.0", content)

    def test_windows_and_macos_xml_and_postinstall(self) -> None:
        wix_xml = REPO_ROOT / "packaging" / "cpack" / "windows" / "wix_patch.xml"
        dist_xml = REPO_ROOT / "packaging" / "cpack" / "macos" / "distribution.xml"
        ET.parse(wix_xml)
        ET.parse(dist_xml)

        postinstall = REPO_ROOT / "packaging" / "cpack" / "macos" / "postinstall.sh"
        with tempfile.TemporaryDirectory() as tmp:
            bundle_dir = pathlib.Path(tmp) / "OpenMedia_Importer.bundle" / "Contents" / "MacOS"
            bundle_dir.mkdir(parents=True, exist_ok=True)
            bin_file = bundle_dir / "OpenMedia_Importer"
            plist_file = pathlib.Path(tmp) / "OpenMedia_Importer.bundle" / "Contents" / "Info.plist"
            dylib_file = pathlib.Path(tmp) / "libOpenMediaCore.dylib"
            bin_file.write_text("bin", encoding="utf-8")
            plist_file.write_text("<plist/>", encoding="utf-8")
            dylib_file.write_text("dylib", encoding="utf-8")
            os.chmod(plist_file, 0o777)

            env = {**os.environ, "MEDIACORE_OVERRIDE_DIR": tmp}
            res = subprocess.run(["bash", str(postinstall)], env=env, check=False)
            self.assertEqual(res.returncode, 0)
            self.assertEqual(stat.S_IMODE(os.stat(plist_file).st_mode), 0o644)
            self.assertEqual(stat.S_IMODE(os.stat(bin_file).st_mode), 0o755)
            self.assertEqual(stat.S_IMODE(os.stat(dylib_file).st_mode), 0o755)


if __name__ == "__main__":
    unittest.main()
