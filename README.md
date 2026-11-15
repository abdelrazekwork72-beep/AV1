# OpenMedia Premiere Pro & Media Encoder Plugin Suite (`v1.1.0` — Influx-v2 Architecture)

Clean-room open-source **AV1 / VP9 / MKV / WebM / MOV / Ogg** Dual-PRM Importer (`OpenMedia_Importer.prm` + `OpenMedia_ImporterHP.prm` / `.bundle`) and Hardware/Software Exporter (`OpenMedia_Exporter.prm` / `.bundle`) powered by `OpenMediaCore` (`C++17` + `FFmpeg 7.1/8.0`) for **Adobe Premiere Pro**, **Adobe Media Encoder**, **After Effects**, and **Audition**.

---

## Key Architectural Highlights (Influx-v2 + Voukoder Pro Parity)

- **Module 1 — `OpenMediaCore` Shared Engine & Hardened Build Toolchain (`OpenMediaCore.dll` / `.dylib` / `.so`)**:
  - **Dual-PRM Shim Build Targets**: Compiles both `OpenMedia_Importer.prm` (Normal Priority `0x00`, priority `100`) and `OpenMedia_ImporterHP.prm` (High-Priority Override `0x01`, priority `200`, `OPENMEDIA_HIGH_PRIORITY_SHIM=1`) with Release **LTCG / IPO**, **AVX2 / SSE4.1** SIMD acceleration, and Windows PE security flags (`/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA`).
  - **WebM / AV1 / VP9 Last-Frame Retention & Zero-PTS Normalization**: Drains 100% of EOF packets so the final frame in WebM/MKV (Chrome recordings, YouTube AV1/VP9) is never dropped, normalizes non-zero start timestamps (`start_time > 0` $\to$ `PTS = 0`), and rejects corrupt video-less containers with `"No decodable video frames in file"`.
  - **Hybrid GPU/CPU Video Decoding & `drainBeforeFallback()`**: Supports `NVDEC`, `D3D11VA`, and Apple Silicon `VideoToolbox` hardware contexts with automatic frame draining and graceful fallback to `libdav1d` / `libvpx-vp9`.
  - **Fault-Tolerant Audio & 5/6-Channel MOV PCM**: Supports `Opus`, `FLAC`, `Vorbis`, `PCM`, `xHE-AAC`, `AAC`, `MP3`, `AC3`, and `DTS`. Substitutes zero-filled planar float silence (`0.0f`) for isolated corrupt packets or empty audio tracks instead of aborting import, supports 5/6-channel MOV PCM (ARRI AMIRA ProRes), and immediately unlocks OS file handles on error.

- **Module 2 — Dual-PRM MediaCore Importer (`OpenMedia_Importer.prm` & `OpenMedia_ImporterHP.prm`)**:
  - **PiPL Trailing-Space Registration Trick**: Registers `'OpenMedia Importer'` for Normal mode and `'OpenMedia Importer '` (with a trailing space) for High-Priority mode (`xOpenMediaImportEntry(..., isHighPriority)`) so Adobe hosts load both shims simultaneously without FourCC/PiPL collision.
  - **`Config.toml` & `!` Filename Suffix Modifiers**:
    - Parses `%APPDATA%/OpenMedia/Config.toml` (`disable`, `overrideAdobe`, `overrideExtensions`, `gpuDecodeMode = "recommended" | "available" | "disabled"`, `maxFrameCacheSizeMB = 400`, `maxPercentUsageAvailableMemory = 50`, `reduceWarnings`, `thumbnailTimecode`, `durationCalculation`).
    - Parses per-file modifiers after the last `!` before the extension: `!w=`, `!h=` (auto-preserving aspect ratio), `!vs=`, `!as=` (GoPro MAX 360 multi-stream selection), `!pf=` (`argb`, `bgra`, `bgra64`, `yuv420p`, `yuv420p10u`, `uyvy422`, `vuya`), `!fr=24000d1001`, `!cr=full|limited`, `!accurate_duration`, and `!rft=1`.
  - **Direct GPU/SIMD Color Conversion & Extended Camera Color Spaces**: Fast-path `NV12` / `P010LE` $\to$ `BGRA8` / `RGB444_10U` conversion with `sRGB`, `BT.601`, `BT.709`, `BT.2020`, `BT.2100 HLG/PQ`, `DCDM X'Y'Z'`, and Sony `SGamut/SLog2`, `SGamut3/SLog3`, and `Cine/SLog3` transfer curves.
  - **Smart GPU Decode Policy Engine (`evaluateDecodePolicy`) & SQLite 3 `STRICT` Thumbnail Cache**:
    - Routes clips `< 2560x1600` (`4,096,000` px) to CPU to avoid GPU init overhead, and routes clips `>= 2560x1600` to GPU up to a process-wide atomic cap of **4 concurrent GPU streams** (`kMaxConcurrentGpuDecoders = 4`), auto-routing the 5th+ stream to CPU.
    - Persists Media Browser thumbnails in a `STRICT` SQLite 3 schema (`tableVersion` and `thumbnails` tables) and enforces exact open-GOP / Long-GOP keyframe seeking.

- **Module 3 — MediaCore Exporter (`OpenMedia_Exporter.prm` / `.bundle`)**:
  - **Out-of-Process JSON IPC Bridge (`PopupIpcBridge`)**: Exchanges `json:`-prefixed messages over `StdIn`/`StdOut` (`--popup --owner <HWND>`) for `Config.toml` settings, live `GPU Status` monitoring, and `BT.2100 PQ/HLG` & `Sony S-Log3` color metadata without Qt GUI conflicts inside Premiere Pro.
  - **Zero-PTS Normalization, 100% EOF Frame Drain & Hardware `drainBeforeFallback()`**: Ensures exported MKV/WebM/MP4 files start at `PTS = 0`, retain 100% of trailing GOP frames, and seamlessly fall back from `NVENC` (driver $\ge 570.0$), `AMF`, `QSV`, or `Vulkan` to `libsvtav1` / `libvpx-vp9` with once-per-session warning deduplication (`PrSDKErrorSuite`).

- **Module 4 — Host Integration Harness, Dual-Policy Benchmarks & Packaging**:
  - **52 Automated CTest Suites**: Verifies Dual-PRM loading (`OpenMedia_Importer.prm` + `OpenMedia_ImporterHP.prm`), `< 2560x1600` CPU vs `>= 2560x1600` GPU policy routing (including 5th-stream CPU overflow), `< 50ms` p95 scrub latency, `Config.toml` deployment, and `VCRUNTIME140.dll` / `VCRUNTIME140_1.dll` runtime verification.

---

## Installation

### Windows (`x64`)
1. Download `OpenMedia-PremierePlugin-v1.0.0-Windows-x64.zip` from [GitHub Releases](https://github.com/abdelrazekwork72-beep/AV1/releases).
2. Extract the archive and run `Install-MediaCore.ps1` as **Administrator** (which verifies `VCRUNTIME140.dll` / `VCRUNTIME140_1.dll` and copies `OpenMedia_Importer.prm`, `OpenMedia_ImporterHP.prm`, `OpenMedia_Exporter.prm`, `OpenMediaCore.dll`, and `Config.toml` to):
   ```text
   C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore
   ```

### macOS (`Universal / Apple Silicon M1+ & Intel`)
1. Download `OpenMedia-PremierePlugin-v1.0.0-macOS.zip` from [GitHub Releases](https://github.com/abdelrazekwork72-beep/AV1/releases).
2. Extract the archive and run `./install-mediacore.sh` (which installs `OpenMedia_Importer.bundle`, `OpenMedia_ImporterHP.bundle`, `OpenMedia_Exporter.bundle`, `libOpenMediaCore.dylib`, and `Config.toml` to):
   ```text
   /Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore
   ```
