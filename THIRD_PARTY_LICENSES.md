# Third-Party Licenses & SPDX Boundary Notice

OpenMedia maintains strict dynamic-linking separation between MIT/Apache-2.0
MediaCore connector plugins (`OpenMedia_Importer`, `OpenMedia_Exporter`) and
LGPL-2.1-or-later / GPL-3.0-or-later / BSD core media libraries (`libOpenMediaCore`).

| Component | SPDX License Identifier | Role |
| :--- | :--- | :--- |
| `OpenMedia_Importer` | `MIT OR Apache-2.0` | Connector Plugin (Dynamic Link) |
| `OpenMedia_Exporter` | `MIT OR Apache-2.0` | Connector Plugin (Dynamic Link) |
| `OpenMediaCore` | `LGPL-2.1-or-later OR GPL-3.0-or-later` | Shared Media Engine |
| `FFmpeg` | `LGPL-2.1-or-later OR GPL-3.0-or-later` | Demuxer / Muxer / Codec Pipeline |
| `dav1d` | `BSD-2-Clause` | AV1 Software Decoder |
| `SVT-AV1` | `BSD-3-Clause-Clear` | AV1 Software Encoder |
| `libvpx` | `BSD-3-Clause` | VP9 Codec |
| `opus` | `BSD-3-Clause` | Opus Audio Codec |
| `flac` | `BSD-3-Clause` | FLAC Audio Codec |
