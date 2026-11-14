# User Story Specification Kit: US-3.2.0

## Multi-Pass Sequence Render Pipeline & Audio Pull Orchestration

| Metadata | Value |
| :--- | :--- |
| **Story ID** | `US-3.2.0` |
| **Module ID** | `module-3-exporter-plugin` |
| **Feature Name** | Multi-Pass Timeline Render Engine, Audio Pull Synchronization & GOP Flusher |
| **Target Binary** | `OpenMedia_Exporter.prm` |
| **Story Score ($S_{\text{story}}$)** | **2.65 / 5.00** $\to$ **$K = 5$ Implementation Units** |
| **Branch Convention** | `feat/us-3.2.0-export-render-engine` |
| **Unit Branches** | `feat/us-3.2.0/unit-N-<slug>` ($N \in [1..5]$) |
| **PR Hierarchy** | Units $1..5 \to$ Story PR $\to$ `feat/module-3-exporter-plugin` |

---

## 1. Spec Kit Specifications

### 1.1 Narrative, Persona & Business Value
- **Persona**: Post-Production Video Editor exporting a feature-length 4K documentary with multi-pass rate control and multi-channel audio mix.
- **Narrative**: During the export phase in Premiere Pro or Adobe Media Encoder, the rendering loop must orchestrate video frame rendering, audio block extraction, encoder packet dispatch, and multiplexing into the target file on disk. Depending on the host version and sequence configuration, the host may drive rendering via an iterative push model (`exSelExport` returning `exportReturn_IterateExporterNoErr`) or the exporter may pull frames directly via `PrSDKSequenceRenderSuite`. By supporting both models seamlessly, interleaving 32-bit float planar audio, monitoring user cancellation via a responsive watchdog, and guaranteeing a clean GOP-tail flush upon completion, OpenMedia ensures flawless, crash-free export jobs every time.
- **Business Value**: Eliminates export failures, timeline render freezes, and audio desynchronization, delivering dependable output for broadcast and streaming deliverables.

#### 1.1.1 Explicit Preconditions
1. User initiates Export in Premiere Pro or starts a queue item in Adobe Media Encoder.
2. `exSelBeginInstance` validates output file path and opens the output Matroska / WebM file handle.
3. Codec encoders and multiplexers (`US-3.3.0`) are instantiated and configured.

#### 1.1.2 Explicit Postconditions
1. Sequence video frames are rendered and encoded at the exact timeline timebase without frame dropping or duplicated timestamps.
2. Audio blocks are extracted synchronously with video and multiplexed with zero audio/video clock drift ($\le 1\,\text{audio sample}$).
3. When the user clicks "Cancel" in Premiere Pro/AME, the render loop stops immediately, releases all memory and GPU textures, and deletes temporary files.
4. On export completion, all delayed B-frames in the encoder queue are flushed and the container header/cues are written cleanly to disk.

---

### 1.2 Gherkin Acceptance Criteria

```gherkin
Feature: Export Sequence Rendering, Audio Pull and Multiplexing

  Scenario: Sequential video and audio multiplexing
    Given an active export job with 1,000 video frames and stereo 48kHz audio
    When exSelExport runs through the render loop
    Then video frames are rendered and encoded sequentially
    And audio samples are interleaved with matching PTS/DTS stamps
    And total multiplexing time overhead is under 5% of render time.

  Scenario: Multi-pass encoding pass coordination
    Given 2-pass VBR selected for AV1 export
    When Pass 1 starts
    Then frames are analyzed and first-pass statistics log is generated
    When Pass 2 starts
    Then statistics log is consumed to optimize rate control and final output file is written.

  Scenario: User cancellation during render
    Given an export job at 45% completion
    When the user clicks "Cancel" in Adobe Media Encoder
    Then ProgressAndCancelWatchdog detects abort flag within 100ms
    And all encoder threads and temporary file handles are terminated cleanly.

  Scenario: GOP tail flushing at sequence end
    Given an AV1 encoder instance holding 8 delayed B-frames in its internal lookahead queue
    When the last timeline frame is delivered
    Then GopTailFlusher sends flush packets (NULL frame) to avcodec_send_frame
    And consumes all trailing packets until avcodec_receive_packet returns AVERROR_EOF
    And updates the Matroska cues index before closing the file.
```

---

### 1.3 Test Cases & Traceability Matrix

| Test ID | Category | Description | Covered Unit | Expected Result |
| :--- | :--- | :--- | :--- | :--- |
| `TC-3.2-01` | Render Loop | Iterative Push-model export cycle | `Unit-1` | Iterates frame by frame returning `exportReturn_IterateExporterNoErr` |
| `TC-3.2-02` | Sequence Pull | Pull-model rendering via `PrSDKSequenceRenderSuite` | `Unit-2` | Pulls timeline frames into `PPixHand` with accurate timecode |
| `TC-3.2-03` | Audio Pull | Audio block extraction and interleaving | `Unit-3` | Continuous 48kHz audio pulled and muxed without gaps or pops |
| `TC-3.2-04` | Watchdog | Cancellation detection and resource cleanup | `Unit-4` | Cancels within 100ms; zero file handle or memory leaks |
| `TC-3.2-05` | Flush | GOP tail flush and Matroska cues finalization | `Unit-5` | All trailing frames muxed; container seek index verified via ffprobe |

---

## 2. Technical Implementation Plan

### 2.1 Domain Invariants, Data Models & Storage Schema

```cpp
namespace OpenMedia::Exporter::Render {

struct RenderProgressInfo {
    int64_t currentFrame;
    int64_t totalFrames;
    float currentPercent;
    std::chrono::steady_clock::time_point startTime;
    bool isCancelled;
};

struct ExportRenderContext {
    csSDK_int32 exportID;
    prPluginID pluginID;
    PrSDKSequenceRenderSuite* renderSuite;
    PrSDKExportProgressSuite* progressSuite;
    PrSDKMemorySuite* memorySuite;
    PrSDKPPixSuite* ppixSuite;
    
    // Core references
    std::shared_ptr<OpenMedia::Core::DemuxEngine> muxer;
    std::shared_ptr<OpenMedia::Exporter::Codecs::IVideoEncoder> videoEncoder;
    std::shared_ptr<OpenMedia::Exporter::Codecs::IAudioEncoder> audioEncoder;
};

} // namespace OpenMedia::Exporter::Render
```

### 2.2 Sequence Diagram: Export Render Loop & Cancellation Watchdog

```mermaid
sequenceDiagram
    autonumber
    participant Host as Premiere Pro / AME
    participant Loop as PushModelRenderLoop
    participant Puller as PullModelSequenceRenderer
    participant Audio as AudioPullOrchestrator
    participant Flusher as GopTailFlusher
    participant Watchdog as ProgressAndCancelWatchdog

    Host->>Loop: exSelExport(context)
    loop While Frame < TotalFrames
        Loop->>Watchdog: CheckCancelStatus()
        alt Cancelled by User
            Watchdog-->>Loop: ABORT
            Loop->>Flusher: EmergencyAbort()
            Loop-->>Host: exportReturn_Abort
        else Normal Execution
            Loop->>Puller: RenderCurrentFrame(frameIndex)
            Puller-->>Loop: PPixHand (Surface)
            Loop->>Loop: EncodeAndMuxVideo(PPixHand)
            Loop->>Audio: PullAudioChunk(sampleTime)
            Audio-->>Loop: FloatAudioData
            Loop->>Loop: EncodeAndMuxAudio(FloatAudioData)
            Loop->>Watchdog: UpdateProgress(frameIndex / totalFrames)
            Watchdog-->>Host: PrSDKExportProgressSuite::UpdateProgress()
            Loop-->>Host: exportReturn_IterateExporterNoErr
        end
    end
    Loop->>Flusher: FlushEncoderBuffers()
    Flusher->>Flusher: WriteContainerCuesAndTrailer()
    Loop-->>Host: exportReturn_IterateExporterDone
```

---

## 3. Expected Folder Architecture at the Project

```
plugins/exporter/
├── include/
│   ├── OpenMedia_PushRenderLoop.h
│   ├── OpenMedia_SequenceRenderer.h
│   ├── OpenMedia_AudioPullOrchestrator.h
│   ├── OpenMedia_ProgressWatchdog.h
│   └── OpenMedia_GopTailFlusher.h
├── src/
│   ├── OpenMedia_PushRenderLoop.cpp
│   ├── OpenMedia_SequenceRenderer.cpp
│   ├── OpenMedia_AudioPullOrchestrator.cpp
│   ├── OpenMedia_ProgressWatchdog.cpp
│   └── OpenMedia_GopTailFlusher.cpp
└── tests/
    ├── Test_PushRenderLoop.cpp
    ├── Test_SequenceRenderer.cpp
    ├── Test_AudioPullOrchestrator.cpp
    ├── Test_ProgressWatchdog.cpp
    └── Test_GopTailFlusher.cpp
```

---

## 4. Implementation Units in Detail & Specialist Agent Assignment

### 4.1 Unit Sizing Decision Matrix & Score Calculation

$$S_{\text{story}} = 0.35 D_1 + 0.25 D_2 + 0.20 D_3 + 0.20 D_4$$
- $D_1 = 3$ (Push/Pull render loop, audio pull, watchdog/cancel, GOP tail flush)
- $D_2 = 3$ (~900 LOC across render loop components)
- $D_3 = 3$ (Multi-pass coordination, PTS/DTS timeline synchronization, cancelation handling)
- $D_4 = 2$ (Render loop, audio puller, muxer bridge)
$$S_{\text{story}} = (0.35 \times 3) + (0.25 \times 3) + (0.20 \times 3) + (0.20 \times 2) = 1.05 + 0.75 + 0.60 + 0.40 = 2.80 \implies K = 5$$

### 4.2 Dynamic $K$ Implementation Unit Specifications

#### Unit 1: Host-Driven Iterative Push Render Loop
- **ID**: `unit-1`
- **Branch**: `feat/us-3.2.0/unit-1-push-render-loop`
- **Objective**: Implements iterative `exSelExport` loop returning `exportReturn_IterateExporterNoErr` for step-by-step frame processing.
- **Files Owned**:
  - `plugins/exporter/include/OpenMedia_PushRenderLoop.h`
  - `plugins/exporter/src/OpenMedia_PushRenderLoop.cpp`
  - `plugins/exporter/tests/Test_PushRenderLoop.cpp`
- **Verification**: `ctest -R Test_PushRenderLoop` tests iterative loop execution and state retention across calls.

#### Unit 2: Plugin-Driven Sequence Renderer (PrSDKSequenceRenderSuite)
- **ID**: `unit-2`
- **Branch**: `feat/us-3.2.0/unit-2-sequence-renderer`
- **Objective**: Pulls sequence frames directly using Adobe Sequence Render Suite with support for multi-threaded asynchronous frame rendering.
- **Files Owned**:
  - `plugins/exporter/include/OpenMedia_SequenceRenderer.h`
  - `plugins/exporter/src/OpenMedia_SequenceRenderer.cpp`
  - `plugins/exporter/tests/Test_SequenceRenderer.cpp`
- **Verification**: `ctest -R Test_SequenceRenderer` tests frame pull requests with simulated mock render suite.

#### Unit 3: Synchronized Audio Pull & Interleaving Orchestrator
- **ID**: `unit-3`
- **Branch**: `feat/us-3.2.0/unit-3-audio-pull-orchestrator`
- **Objective**: Pulls timeline audio blocks, converts to 32-bit float planar, and synchronizes interleaving with video packets.
- **Files Owned**:
  - `plugins/exporter/include/OpenMedia_AudioPullOrchestrator.h`
  - `plugins/exporter/src/OpenMedia_AudioPullOrchestrator.cpp`
  - `plugins/exporter/tests/Test_AudioPullOrchestrator.cpp`
- **Verification**: `ctest -R Test_AudioPullOrchestrator` verifies audio sample extraction and zero PTS drift against video frames.

#### Unit 4: Export Progress & Cancellation Watchdog
- **ID**: `unit-4`
- **Branch**: `feat/us-3.2.0/unit-4-progress-watchdog`
- **Objective**: Updates Premiere Pro/AME progress bar, calculates remaining time, and monitors host cancellation signals.
- **Files Owned**:
  - `plugins/exporter/include/OpenMedia_ProgressWatchdog.h`
  - `plugins/exporter/src/OpenMedia_ProgressWatchdog.cpp`
  - `plugins/exporter/tests/Test_ProgressWatchdog.cpp`
- **Verification**: `ctest -R Test_ProgressWatchdog` tests cancel event propagation within $< 100\,\text{ms}$.

#### Unit 5: GOP Tail Flusher & Container Trailer Finalizer
- **ID**: `unit-5`
- **Branch**: `feat/us-3.2.0/unit-5-gop-tail-flusher`
- **Objective**: Flushes internal encoder delayed packets upon end-of-sequence and finalizes Matroska/WebM Cues, SeekHead, and Duration.
- **Files Owned**:
  - `plugins/exporter/include/OpenMedia_GopTailFlusher.h`
  - `plugins/exporter/src/OpenMedia_GopTailFlusher.cpp`
  - `plugins/exporter/tests/Test_GopTailFlusher.cpp`
- **Verification**: `ctest -R Test_GopTailFlusher` verifies all buffered frames are drained and container trailer is valid.

### 4.3 Specialist Agent Assignment

| Unit | Subagent Type | Specialist Role |
| :--- | :--- | :--- |
| `unit-1` | `agency-minimal-change-engineer` | Exporter Render Loop Specialist |
| `unit-2` | `agency-desktop-app-engineer` | Premiere Sequence Render Suite Specialist |
| `unit-3` | `agency-voice-ai-integration-engineer` | Audio Extraction & Synchronization Specialist |
| `unit-4` | `agency-developer-tooling-engineer` | Progress Watchdog & Event Specialist |
| `unit-5` | `agency-video-streaming-engineer` | Encoder Flushing & Container Finalization Specialist |

### 4.4 Shared Base Contract & Seeding Plan
- `ExportRenderContext` and `RenderProgressInfo` seeded in `OpenMedia_RenderTypes.h`.
- Clear contract between render loop and video/audio codec backends.

---

## 5. Security Gateway powered by `security-audit`

### 5.1 Trust Boundaries & Actors
- **Host Callbacks**: Premiere Pro render suite function pointers. Must be checked for non-null before invocation.
- **File System**: Output file stream writing encoded packets.

### 5.2 STRIDE Threat Matrix & ASVS 5.0 Controls

| Threat Category | Scenario | Mitigation / ASVS 5.0 Control |
| :--- | :--- | :--- |
| **Denial of Service** | Infinite loop if host sequence fails to advance frames | Safety timeout counter ($10^6$ frames max or progress watchdog abort) |
| **Tampering** | Partial/corrupted file left behind on user abort | Immediate unlink/cleanup of partially written output file on abort |
| **Elevation of Privilege** | Null pointer dereference when accessing uninitialized host suite | Strict null-check assertions on all Adobe Suite pointers |

### 5.3 Global Cryptographic, Jurisdictional & Data Protection Controls
- Temporary files created during multi-pass rendering are securely unlinked and cleared using OS-level secure deletion.

### 5.4 Automated Security Verification Checklist
- [x] Null pointer check verified across all Adobe Suite interfaces.
- [x] Cancellation test confirms immediate release of all file descriptors and memory.

### 5.5 Gate G2 Sign-Off Checklist
- [x] No lingering zombie threads or worker processes after export completion or cancel.
- [x] Zero resource leaks under ASAN / Valgrind during interrupted export workflows.

---

## 6. Validation Gates & Acceptance Sign-Off

| Gate | Criterion | Verification Command | Pass Condition |
| :--- | :--- | :--- | :--- |
| **Gate G0** | Compilation & Formatter | `cmake --build --preset windows-x64-release` | 0 errors, 0 warnings |
| **Gate G1** | Verification Coverage | `ctest -R Test_Render*` | 100% tests pass; coverage $\ge 90\%$ |
| **Gate G2** | Security Audit | `scripts/security-audit-scan.sh --target render-engine` | 0 high/critical issues |
| **Gate G-Complexity** | Cyclomatic Complexity | `python scripts/check-complexity.py plugins/exporter/render` | $CC \le 6$ target, $CC \le 10$ max ceiling |
| **Gate G-Docs** | API Documentation | Doxygen comment check | 100% render engine functions documented |
| **Gate G-Review** | Independent Council | GitHub PR Conversation Sign-Off | Approval comment from Code Reviewer Council |
