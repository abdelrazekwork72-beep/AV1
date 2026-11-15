# OpenMedia Premiere Pro & Media Encoder Plugin (`v1.0.0`)

Clean-room open-source **AV1 / VP9 / MKV / WebM / MOV / MP4 / Ogg** Importer (`OpenMedia_Importer.prm`, `OpenMedia_ImporterHP.prm` / `.bundle`) and Exporter (`OpenMedia_Exporter.prm` / `.bundle`) powered by `OpenMediaCore` (`C++17`) for **Adobe Premiere Pro**, **Adobe Media Encoder**, **After Effects**, and **Audition**.

## Features & Influx v1.7.1 / v2.0.0 Beta 1 Parity

- **Module 1 — `OpenMediaCore` Engine (`OpenMediaCore.dll` / `.dylib` / `.so`)**:
  - Thread-safe RAII wrappers around `libavformat`, `libavcodec`, `libswscale`, `libswresample`, and `libavutil` with Release `LTCG`, `AVX2`/`SSE4.1` SIMD, and Windows PE security flags (`ASLR`, `DEP/NX`, `High Entropy VA`).
  - WebM (VP9/AV1, Chrome, YouTube) final-frame EOF retention, non-zero start-timestamp normalization (`frame 0`), and empty-video guard (`"No decodable video frames in file"`).
  - Fault-tolerant audio (`Opus`, `FLAC`, `Vorbis`, `xHE-AAC`, `AAC`, `MP3`, `AC3`, `DTS`, and 5/6-channel MOV `PCM` such as ARRI AMIRA ProRes) with corrupt-packet silence substitution, silent track synthesis, and immediate file unlock on Windows.
- **Module 2 — Dual-Shim Importer (`OpenMedia_Importer.prm` & `OpenMedia_ImporterHP.prm` / `.bundle`)**:
  - Dual-PRM shim architecture: Normal priority (`0x00`, PiPL `"OpenMedia Importer"`) and High-Priority override (`0x01`, PiPL `"OpenMedia Importer "` with trailing space, `priority = 100`).
  - Smart GPU Decode Policy Engine (`evaluateDecodePolicy`): supports `gpuDecodeMode` (`"recommended"`, `"available"`, `"disabled"`), enforces a $\ge 2560 	imes 1600$ ($4	ext{ MP}$) resolution threshold in recommended mode, caps concurrent GPU-decoded streams at **4 per Adobe host process**, and executes `drainBeforeFallback()` to CPU (`libdav1d` / `libvpx`) on hardware faults.
  - Direct GPU/SIMD `NV12` & `P010LE` $	o$ `BGRA` color conversion and camera gamuts (`sRGB`, `BT.601`, `BT.709`, `BT.2020`, `BT.2100 HLG/PQ`, `DCDM X'Y'Z'`, `Sony SGamut/SLog2`, `SGamut3/SLog3`, `Cine/SLog3`).
  - Advanced `%APPDATA%\OpenMedia\Config.toml` preferences, `!` filename suffix modifiers (`!w=`, `!h=`, `!vs=`, `!as=`, `!pf=`, `!fr=24000d1001`, `!cr=full|limited`, `!accurate_duration`, `!rft=1`), and SQLite 3 `STRICT` Thumbnail Cache (`tableVersion` & `thumbnails`).
- **Module 3 — `OpenMedia_Exporter` (`OpenMedia_Exporter.prm` / `.bundle`)**:
  - Software & hardware encoders: `SVT-AV1`, `libaom-av1`, `libvpx-vp9`, `NVENC` (driver $\ge 570.0$, `LOAD_LIBRARY_SEARCH_SYSTEM32` hardened), `AMF`, `QSV`, and Vulkan Video with `drainBeforeFallback()`.
  - `Match Source` intelligence, dynamic UI parameter validation (`rebuildAllParams`), RFC 5646 (`eng`/`ara`) closed-caption embedding, and Out-of-Process JSON IPC (`--popup --owner HWND` over `StdIn`/`StdOut` with `json:` prefix).
- **Module 4 — Host Integration Harness & Packaging**:
  - Standalone `DeploymentValidator` (verifying both `.prm` shims), `ScrubLatencyBenchmark` ($p95 < 50	ext{ ms}$ & 4-GPU concurrency cap), `AmeQueueValidator`, and automated CPack / GitHub Actions release packaging with `VCRUNTIME140.dll` verification.

## Installation

### Windows (`x64`)
1. Download `OpenMedia-PremierePlugin-v1.0.0-Windows-x64.zip` from [GitHub Releases](https://github.com/abdelrazekwork72-beep/AV1/releases).
2. Extract the archive and run `Install-MediaCore.ps1` as **Administrator**, or copy `OpenMedia_Importer.prm`, `OpenMedia_ImporterHP.prm`, `OpenMedia_Exporter.prm`, and `OpenMediaCore.dll` directly to:
   ```text
   C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore
   ```

### macOS (`Universal / Apple Silicon & Intel`)
1. Download `OpenMedia-PremierePlugin-v1.0.0-macOS.zip` from [GitHub Releases](https://github.com/abdelrazekwork72-beep/AV1/releases).
2. Extract the archive and run `./install-mediacore.sh`, or copy `OpenMedia_Importer.bundle`, `OpenMedia_Exporter.bundle`, and `libOpenMediaCore.dylib` to:
   ```text
   /Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore
   ```
