#!/usr/bin/env python3
"""SPDX dual-license and proprietary Adobe SDK header compliance auditor."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from typing import List

IGNORED_DIRS = {".git", "build", "vcpkg_installed", ".worktrees", "__pycache__"}
MOCK_SDK_PREFIX = ("tests", "mocks", "mock_sdk")
CONNECTOR_PREFIXES = (
    ("src", "importer"),
    ("src", "exporter"),
    ("include", "importer"),
    ("include", "exporter"),
)
ADOBE_BANNER_MARKERS = (
    "ADOBE CONFIDENTIAL",
    "Copyright Adobe Systems Incorporated",
    "Adobe Premiere Pro SDK",
)
FFMPEG_HEADER_PATTERN = re.compile(
    r'^\s*#\s*include\s*[<"](libav(codec|format|util|filter)|libsw(scale|resample))',
    re.MULTILINE,
)


def is_ignored_path(path: pathlib.Path) -> bool:
    """Return True if path resides inside an ignored build or VCS directory."""
    return any(part in IGNORED_DIRS for part in path.parts)


def is_mock_sdk_header(path: pathlib.Path, root: pathlib.Path) -> bool:
    """Return True if path is an intentional clean-room mock header under tests/mocks/mock_sdk."""
    try:
        rel = path.relative_to(root)
    except ValueError:
        return False
    return rel.parts[:3] == MOCK_SDK_PREFIX


def is_connector_source(path: pathlib.Path, root: pathlib.Path) -> bool:
    """Return True if path belongs to MIT/Apache-2.0 connector layers (importer/exporter)."""
    try:
        rel = path.relative_to(root)
    except ValueError:
        return False
    return rel.parts[:2] in CONNECTOR_PREFIXES


def check_file_for_proprietary_markers(path: pathlib.Path, root: pathlib.Path) -> List[str]:
    """Return a list of violation descriptions for a single file."""
    violations: List[str] = []
    if is_mock_sdk_header(path, root):
        return violations
    if path.name.startswith("PrSDK") and path.suffix in {".h", ".hpp"}:
        violations.append(f"Proprietary SDK header name detected: {path}")
        return violations
    if path.suffix in {".h", ".hpp", ".c", ".cpp", ".cmake"}:
        text = path.read_text(encoding="utf-8", errors="ignore")
        for marker in ADOBE_BANNER_MARKERS:
            if marker in text:
                violations.append(f"Proprietary Adobe banner '{marker}' in {path}")
        if is_connector_source(path, root) and FFMPEG_HEADER_PATTERN.search(text):
            violations.append(
                f"Dynamic-linking SPDX boundary violation (direct FFmpeg include in connector): {path}"
            )
    return violations


def audit_directory(root: pathlib.Path) -> List[str]:
    """Scan root recursively for proprietary Adobe SDK headers or license boundary violations."""
    violations: List[str] = []
    for path in root.rglob("*"):
        if not path.is_file() or is_ignored_path(path):
            continue
        violations.extend(check_file_for_proprietary_markers(path, root))
    return violations


def main(argv: List[str] | None = None) -> int:
    """CLI entrypoint returning 0 if compliant, 1 if violations exist."""
    parser = argparse.ArgumentParser(description="Audit OpenMedia tree for Adobe SDK leaks.")
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path("."))
    args = parser.parse_args(argv)
    violations = audit_directory(args.root.resolve())
    if violations:
        for item in violations:
            print(f"[FAIL] {item}", file=sys.stderr)
        return 1
    print("[PASS] 0 proprietary Adobe SDK headers and 0 SPDX boundary violations detected.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
