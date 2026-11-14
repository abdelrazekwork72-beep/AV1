#!/usr/bin/env python3
"""TC-1.1.2: Verify vcpkg.json and vcpkg-configuration.json manifest & FFmpeg pinning."""

import json
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
HEX_SHA_RE = re.compile(r"^[0-9a-fA-F]{40}$")
EXPECTED_BASELINE = "382a945938bf804c0ecdd61c94297b7b2de2dc3b"
REQUIRED_FFMPEG_FEATURES = {
    "avcodec",
    "avformat",
    "swscale",
    "swresample",
    "avutil",
    "dav1d",
    "svtav1",
    "vpx",
    "opus",
    "flac",
    "nvcodec",
    "vulkan",
}


def parse_semver(version_str: str) -> tuple[int, ...]:
    base_version = version_str.split("#", 1)[0].strip()
    parts = base_version.split(".")
    return tuple(int(p) for p in parts)


class TestVcpkgManifest(unittest.TestCase):
    """TC-1.1.2 validation suite for vcpkg manifest and registry configuration."""

    def test_vcpkg_json_valid_and_pinned(self) -> None:
        manifest_path = REPO_ROOT / "vcpkg.json"
        self.assertTrue(manifest_path.is_file(), f"Missing {manifest_path}")

        with manifest_path.open("r", encoding="utf-8") as f:
            manifest = json.load(f)

        self.assertEqual(
            manifest.get("$schema"),
            "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
        )
        self.assertEqual(manifest.get("name"), "openmedia-premiere-plugin")
        self.assertEqual(manifest.get("version-string"), "1.0.0")
        self.assertEqual(manifest.get("license"), "MIT AND LGPL-2.1-or-later")

        baseline = manifest.get("builtin-baseline", "")
        self.assertRegex(baseline, HEX_SHA_RE, "builtin-baseline must be a 40-char hex SHA")
        self.assertEqual(baseline, EXPECTED_BASELINE)

        dependencies = manifest.get("dependencies", [])
        self.assertIsInstance(dependencies, list)

        ffmpeg_deps = [
            dep for dep in dependencies if isinstance(dep, dict) and dep.get("name") == "ffmpeg"
        ]
        self.assertEqual(len(ffmpeg_deps), 1, "Expected exactly one ffmpeg dependency entry")
        ffmpeg = ffmpeg_deps[0]

        version_constraint = ffmpeg.get("version>=")
        self.assertIsInstance(version_constraint, str, "ffmpeg must specify version>=")
        self.assertGreaterEqual(
            parse_semver(version_constraint),
            (7, 1, 0),
            f"ffmpeg version>= must be >= 7.1.0, got {version_constraint}",
        )
        self.assertIs(ffmpeg.get("default-features"), False)

        features = set(ffmpeg.get("features", []))
        missing = REQUIRED_FFMPEG_FEATURES - features
        self.assertFalse(missing, f"Missing required FFmpeg features: {sorted(missing)}")

    def test_vcpkg_configuration_json_valid(self) -> None:
        config_path = REPO_ROOT / "vcpkg-configuration.json"
        self.assertTrue(config_path.is_file(), f"Missing {config_path}")

        with config_path.open("r", encoding="utf-8") as f:
            config = json.load(f)

        default_registry = config.get("default-registry", {})
        self.assertIsInstance(default_registry, dict)
        self.assertEqual(default_registry.get("kind"), "git")
        self.assertEqual(default_registry.get("repository"), "https://github.com/microsoft/vcpkg")

        baseline = default_registry.get("baseline", "")
        self.assertRegex(baseline, HEX_SHA_RE, "default-registry.baseline must be a 40-char hex SHA")
        self.assertEqual(baseline, EXPECTED_BASELINE)


if __name__ == "__main__":
    unittest.main()
