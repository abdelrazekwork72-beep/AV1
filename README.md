# OpenMedia Premiere Pro & Media Encoder Plugin (`v1.0.0`)

Clean-room open-source **AV1 / VP9 / MKV / WebM / Ogg** Importer (`OpenMedia_Importer.prm` / `.bundle`) and Exporter (`OpenMedia_Exporter.prm` / `.bundle`) powered by `OpenMediaCore` (`C++17`) for **Adobe Premiere Pro** and **Adobe Media Encoder**.

## Features

- **Module 1 — `OpenMediaCore` Engine (`OpenMediaCore.dll` / `.dylib` / `.so`)**:
  - Thread-safe RAII wrappers around `libavformat`, `libavcodec`, `libswscale`, `libswresample`, and `libavutil`.
  - Clean-room **LGPL v2.1 / BSD / MIT / ISC** compliance (`--enable-gpl` and `--enable-nonfree` strictly forbidden).
- **Module 2 — `OpenMedia_Importer` (`OpenMedia_Importer.prm` / `.bundle`)**:
  - Native `.mkv`, `.webm`, and `.ogg` container demuxing (`VP9`, `AV1`, `Opus`, `Vorbis`, `FLAC`).
  - Asynchronous `imCreateAsyncImporter` & `aiGetFrame` ring-buffer prefetching with 16-byte SIMD alignment (`PrPixelFormat_BGRA_4444_8u`, `VUYA_4444_8u`, `BGRA_4444_32f`).
  - 49-tap Blackman-Harris polyphase windowed-sinc audio resampler (`> 96 dB` SNR, zero 4-hour drift).
- **Module 3 — `OpenMedia_Exporter` (`OpenMedia_Exporter.prm` / `.bundle`)**:
  - Software & hardware encoders: `SVT-AV1`, `libaom-av1`, `libvpx-vp9`, `NVENC` (`LOAD_LIBRARY_SEARCH_SYSTEM32` hardened), `AMF`, `QSV`, and Vulkan Video.
  - `Match Source` intelligence, dynamic UI parameter validation, and RFC 5646 (`eng`/`ara`) closed-caption track embedding.
- **Module 4 — Host Integration Harness & Packaging**:
  - Standalone `DeploymentValidator`, `ScrubLatencyBenchmark`, `AmeQueueValidator`, and automated CPack / GitHub Actions release packaging.

## Installation

### Windows (`x64`)
1. Download `OpenMedia-PremierePlugin-v1.0.0-Windows-x64.zip` from [GitHub Releases](https://github.com/abdelrazekwork72-beep/AV1/releases).
2. Extract the archive and run `Install-MediaCore.ps1` as **Administrator**, or copy `OpenMedia_Importer.prm`, `OpenMedia_Exporter.prm`, and `OpenMediaCore.dll` directly to:
   ```text
   C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore
   ```

### macOS (`Universal / Apple Silicon & Intel`)
1. Download `OpenMedia-PremierePlugin-v1.0.0-macOS.zip` from [GitHub Releases](https://github.com/abdelrazekwork72-beep/AV1/releases).
2. Extract the archive and run `./install-mediacore.sh`, or copy `OpenMedia_Importer.bundle`, `OpenMedia_Exporter.bundle`, and `libOpenMediaCore.dylib` to:
   ```text
   /Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore
   ```
