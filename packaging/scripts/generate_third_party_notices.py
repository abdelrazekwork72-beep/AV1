#!/usr/bin/env python3
"""Generates THIRD_PARTY_LICENSES.md with SPDX dual-license boundaries."""

from __future__ import annotations

import argparse
import pathlib
import sys
from typing import List, Tuple

COMPONENTS: List[Tuple[str, str, str]] = [
    ("OpenMedia_Importer", "MIT OR Apache-2.0", "Connector Plugin (Dynamic Link)"),
    ("OpenMedia_Exporter", "MIT OR Apache-2.0", "Connector Plugin (Dynamic Link)"),
    ("OpenMediaCore", "LGPL-2.1-or-later OR GPL-3.0-or-later", "Shared Media Engine"),
    ("FFmpeg", "LGPL-2.1-or-later OR GPL-3.0-or-later", "Demuxer / Muxer / Codec Pipeline"),
    ("dav1d", "BSD-2-Clause", "AV1 Software Decoder"),
    ("SVT-AV1", "BSD-3-Clause-Clear", "AV1 Software Encoder"),
    ("libvpx", "BSD-3-Clause", "VP9 Codec"),
    ("opus", "BSD-3-Clause", "Opus Audio Codec"),
    ("flac", "BSD-3-Clause", "FLAC Audio Codec"),
]


def render_notices() -> str:
    """Render Markdown notice table with SPDX identifiers."""
    lines = [
        "# Third-Party Licenses & SPDX Boundary Notice",
        "",
        "OpenMedia maintains strict dynamic-linking separation between MIT/Apache-2.0",
        "MediaCore connector plugins (`OpenMedia_Importer`, `OpenMedia_Exporter`) and",
        "LGPL-2.1-or-later / GPL-3.0-or-later / BSD core media libraries (`libOpenMediaCore`).",
        "",
        "| Component | SPDX License Identifier | Role |",
        "| :--- | :--- | :--- |",
    ]
    for name, spdx, role in COMPONENTS:
        lines.append(f"| `{name}` | `{spdx}` | {role} |")
    lines.append("")
    return "\n".join(lines)


def main(argv: List[str] | None = None) -> int:
    """Write THIRD_PARTY_LICENSES.md to the requested output path."""
    parser = argparse.ArgumentParser(description="Generate THIRD_PARTY_LICENSES.md")
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("THIRD_PARTY_LICENSES.md"))
    args = parser.parse_args(argv)
    args.output.write_text(render_notices(), encoding="utf-8")
    print(f"[PASS] Generated {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
