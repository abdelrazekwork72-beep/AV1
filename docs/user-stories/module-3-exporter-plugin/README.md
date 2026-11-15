# Module 3: MediaCore Exporter Plugin (`OpenMedia_Exporter.prm`)

- **Module Identifier**: `module-3-exporter-plugin`
- **Module Branch**: `feat/module-3-exporter-plugin`
- **Field Profile**: `software` (`.agents/field-profile.json`)
- **Primary Deliverables**:
  1. Standalone MediaCore Exporter connector binary (`OpenMedia_Exporter.prm` on Windows, `OpenMedia_Exporter.bundle` on macOS) implementing `xSDKExport` (Open-source replacement for **Voukoder Pro**).
  2. Multi-container registration via `exSelStartup` returning `exportReturn_IterateExporter` (`Matroska MKV`, `WebM`, `AV1 MP4/Raw`) and support for Timeline Sequence Preview File Formats (`Sequence Settings > Preview File Format`).
  3. Dynamic Parameter UI (`PrSDKExportStdParamSuite` & `PrSDKExportParamSuite`) with `SetParamsVersion()`, `canMatchSource = kPrTrue` (`doConformToMatchParams`), `canEmbedCaptions = kPrTrue` (`PrSDKCaptioningSuite`), and `exSelValidateParamChanged` (`rebuildAllParams = kPrTrue`).
  4. Dual Push Model (`PrSDKExporterUtilitySuite::DoMultiPassExportLoop` with immediate `inRenderedFrame` copy) and Pull Model (`PrSDKSequenceRenderSuite::RenderVideoFrameAndConformToPixelFormat` + `PrSDKSequenceAudioSuite`) rendering, pause/resume (`suiteError_ExporterSuspended` $\to$ `WaitForResume`), and GOP-closing abort handling.
  5. Complete 8/10/12-bit & 4:2:0/4:2:2/4:4:4 encoding across CPU (`libsvtav1`, `libaom`, `libvpx-vp9`, `libopus`, `libflac`) and GPU (`av1_nvenc`/`hevc_nvenc` with NVIDIA driver $\ge 570.0$, AMD `amf`, Intel `qsv`/`vaapi`, FFmpeg 8.0 `vulkan`), CRF/QP/base_q_index normalization, 4:4:4 AV1 Profile 2 guard, and automatic fallback via `PrSDKErrorSuite`.

---

## 1. Module Overview & `xSDKExport` Lifecycle

The Exporter Plugin conforms strictly to the Adobe MediaCore Exporter C++ SDK specification, exposing the single entry point:
```cpp
extern "C" DllExport PREMPLUGENTRY xSDKExport(
    csSDK_int32 selector,
    exportStdParms* stdParms,
    void* param1,
    void* param2
);
```

```mermaid
sequenceDiagram
    autonumber
    participant Host as Premiere Pro / Media Encoder
    participant Exp as xSDKExport (OpenMedia_Exporter)
    participant UI as ExportParamBuilder & Validator
    participant Loop as Push/Pull Render Engine
    participant Codecs as Codec Backends & Fallback Guard
    participant Core as libOpenMediaCore (ICoreMuxer)

    Note over Host,Exp: Phase 1: Multi-Format Startup Iteration (exSelStartup)
    loop Format Index 0..2 (MKV, WebM, AV1 MP4)
        Host->>Exp: exSelStartup(exExporterInfoRec)
        Exp-->>Host: canMatchSource=kPrTrue, canEmbedCaptions=kPrTrue, exportReturn_IterateExporter (then IterateExporterDone)
    end

    Note over Host,UI: Phase 2: Parameter Generation, Match Source & Dynamic Validation
    Host->>Exp: exSelGenerateDefaultParams & exSelPostProcessParams
    Exp->>UI: SetParamsVersion(), build Std/Custom Params, apply Match Source (doConformToMatchParams)
    Host->>Exp: exSelValidateParamChanged
    Exp->>UI: Hide/show HW vs CPU options, enforce 4:4:4 Profile 2 guard, set rebuildAllParams=kPrTrue

    Note over Host,Core: Phase 3: Export Execution (Push Model DoMultiPassExportLoop / Pull Model)
    Host->>Exp: exSelExport(exDoExportRec)
    Exp->>Codecs: Initialize Encoder (NVENC >=570.0 / AMF / QSV / Vulkan / SVT-AV1)
    alt Hardware avcodec_open2 Fails (e.g. False-Positive ASIC)
        Codecs->>Host: Report warning via PrSDKErrorSuite & Demote to libsvtav1 / libvpx-vp9
    end
    Exp->>Loop: PrSDKExporterUtilitySuite::DoMultiPassExportLoop(frameCallback)
    loop Per Rendered Frame in Callback
        Host->>Loop: Deliver PPixHand inRenderedFrame
        Loop->>Loop: Immediately copy/convert inRenderedFrame before callback returns
        Loop->>Codecs: encodeFrame() (base_q_index 0..255 / CQ 0..51 / CRF 0..63)
        Loop->>Core: writeInterleavedPacket() (Video + 32f Planar Audio + Captions)
        Loop->>Host: UpdateProgressPercent()
        opt Host Returns suiteError_ExporterSuspended
            Loop->>Host: PrSDKExportProgressSuite::WaitForResume()
        end
        opt User Aborts (exportReturn_Abort)
            Loop->>Codecs: Request few closing frames, flush GOP, write MKV trailer & exit
        end
    end
    Loop->>Codecs: Flush delayed GOP B-frames (avcodec_send_frame NULL)
    Loop->>Core: writeTrailerAndClose() (Matroska Cues & SeekHead)
```

---

## 2. User Story Index & Dynamic Unit Sizing

| Story ID | Branch | Title | Score ($S_{\text{story}}$) | Unit Count ($K$) | Status |
| :--- | :--- | :--- | :---: | :---: | :---: |
| **`US-3.1.0`** | `feat/us-3.1.0-exporter-registration-params-ui` | Exporter Registration, Parameter UI & Match Source Automation | **3.00** | **5 Units** | `Ready` |
| **`US-3.2.0`** | `feat/us-3.2.0-export-render-engine` | Dual Push/Pull Export Render Loops & Progress Watchdog | **2.65** | **5 Units** | `Ready` |
| **`US-3.3.0`** | `feat/us-3.3.0-codecs-hardware-fallback` | Modern Codec Backends, CRF Normalizer & Graceful Fallback | **3.20** | **6 Units** | `Ready` |

---

## 3. Base Contract Seeding for Module 3 (`include/exporter/exporter_types.hpp`)

To maintain **Invariant B (Zero Inter-Unit Blocking)** across all 3 stories and 16 units in Module 3, the unified base contract is pre-seeded on `feat/module-3-exporter-plugin`:

```cpp
// include/exporter/exporter_types.hpp
#pragma once
#include <openmedia/core_types.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace openmedia::exporter {

using csSDK_int32 = int32_t;
using prPluginID  = int32_t;
using PPixHand    = void*;

constexpr int32_t kOpenMediaExporterParamsVersion = 1001;

enum class ContainerTarget { MatroskaMKV = 0, WebM = 1, MP4_AV1 = 2 };
enum class RenderLoopMode  { PushMultiPassLoop, PullSequenceRender };
enum class RateControlMode { CRF_ConstantQuality, VBR_SinglePass, VBR_TwoPass, CBR };

struct ExportJobSettings {
    ContainerTarget container{ContainerTarget::MatroskaMKV};
    RenderLoopMode loopMode{RenderLoopMode::PushMultiPassLoop};
    VideoEncodeConfig video;
    AudioEncodeConfig audio;
    RateControlMode rateControl{RateControlMode::CRF_ConstantQuality};
    bool matchSource{true};
    bool embedCaptions{false};
    bool rebuildAllParams{false};
    std::string outputFilePathUtf8;
};

struct ExportProgressState {
    int64_t renderedFrames{0};
    int64_t totalFrames{0};
    double percentComplete{0.0};
    bool isSuspended{false};
    bool isAborted{false};
    int abortClosingFramesRemaining{0};
};

struct ExporterInstance {
    prPluginID exporterPluginID{0};
    ExportJobSettings settings;
    ExportProgressState progress;
    std::shared_ptr<ICoreMuxer> muxer;
    std::shared_ptr<ICoreVideoEncoder> videoEncoder;
    std::shared_ptr<ICoreAudioEncoder> audioEncoder;
};

} // namespace openmedia::exporter
```

---

## 4. Cascading Merge & Validation Plan

1. **US-3.1.0**: Units `unit-1..5` merge cascadingly into `feat/us-3.1.0-exporter-registration-params-ui` $\to$ Gate G0..G-Review $\to$ `feat/module-3-exporter-plugin`.
2. **US-3.2.0**: Units `unit-1..5` merge cascadingly into `feat/us-3.2.0-export-render-engine` $\to$ Gate G0..G-Review $\to$ `feat/module-3-exporter-plugin`.
3. **US-3.3.0**: Units `unit-1..6` merge cascadingly into `feat/us-3.3.0-codecs-hardware-fallback` $\to$ Gate G0..G-Review $\to$ `feat/module-3-exporter-plugin`.
4. **Module 3 Promotion**: Module Leader runs all 16 exporter unit tests and merges `feat/module-3-exporter-plugin` into `main`.

---

## 6. Influx-v2 Architecture Upgrades Implemented in Module 3

1. **Out-of-Process JSON IPC Bridge (`PopupIpcBridge`) & HDR/Cinema Color Metadata (`US-3.1.0`)**:
   - `PopupIpcBridge` (`export_param_builder.hpp/.cpp`) exchanges `json:`-prefixed lines over `StdIn`/`StdOut` (`--popup --owner <HWND>`) carrying `Config.toml` paths, `PopupGpuStatus` (`GPU Status` panel), and `ExporterPresetParams` (`ExportColorMetadata`: `BT709`, `BT2100_PQ`, `BT2100_HLG`, `Sony_SLog3`) so external UI tools never conflict with Premiere Pro's internal Qt runtime.
2. **Zero-PTS Normalization & 100% EOF Frame Drain (`US-3.2.0`)**:
   - `PushRenderLoop`, `PullSequenceRenderer`, and `GopTailFlusher` normalize the first exported frame timestamp to `PTS = 0` and drain 100% of trailing GOP frames at sequence EOF so exported WebM/MKV clips never lose their final frame.
3. **Hardware Encoder `drainBeforeFallback()` & Session Warning Deduplication (`US-3.3.0`)**:
   - `ChromaFallbackGuard::drainBeforeFallback()` flushes pending frames before switching from `NVENC`/`AMF`/`QSV`/`Vulkan` to `libsvtav1`/`libvpx-vp9` and uses `FallbackSessionState` to emit at most one warning per session via `PrSDKErrorSuite` (`reduceWarnings`).
