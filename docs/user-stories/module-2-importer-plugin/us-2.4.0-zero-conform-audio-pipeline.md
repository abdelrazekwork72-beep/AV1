# User Story Specification Kit: US-2.4.0

## Zero-Conform Audio Extraction, Peak Waveform Generation & Multi-Channel Routing

| Metadata | Value |
| :--- | :--- |
| **Story ID** | `US-2.4.0` |
| **Module ID** | `module-2-importer-plugin` |
| **Feature Name** | Zero-Conform Audio Decoding, Peak Waveform Synthesis & 32-Channel Layout Routing |
| **Target Binary** | `OpenMedia_Importer.prm` |
| **Story Score ($S_{\text{story}}$)** | **2.80 / 5.00** $\to$ **$K = 5$ Implementation Units** |
| **Branch Convention** | `feat/us-2.4.0-zero-conform-audio-pipeline` |
| **Unit Branches** | `feat/us-2.4.0/unit-N-<slug>` ($N \in [1..5]$) |
| **PR Hierarchy** | Units $1..5 \to$ Story PR $\to$ `feat/module-2-importer-plugin` |

---

## 1. Spec Kit Specifications

### 1.1 Narrative, Persona & Business Value
- **Persona**: Audio Engineer and Video Editor working with high-channel count gameplay captures (OBS Studio / MKV) containing Opus / FLAC audio tracks.
- **Narrative**: Traditionally, importing MKV/WebM files with Opus or FLAC audio into editing suites forces the application to "conform" the audio—decoding entire hour-long tracks to 32-bit uncompressed `.cfa` and `.pek` cache files on disk. This blocks immediate timeline playback, consumes dozens of gigabytes of SSD space, and wears down drive health. By intercepting `imGetInfo8` to signal `avoidAudioConform = kPrTrue` with `kRandomAccessImport`, extracting 32-bit floating point planar PCM on the fly, responding to `imGetPeakAudio` with instantaneous peak waveform pyramids, and routing up to 32 discrete surround/spatial channels, OpenMedia enables instant drag-and-drop audio playback and visible waveforms with zero disk conforming delay.
- **Business Value**: Eliminates 100% of the conforming wait time on MKV imports, saving video editors 5–15 minutes per imported asset and preserving gigabytes of local storage.

#### 1.1.1 Explicit Preconditions
1. Container demuxing engine identifies one or more audio streams encoded in Opus, FLAC, Vorbis, or PCM.
2. Adobe Premiere Pro initiates `imGetInfo8`, `imImportAudio`, or `imGetPeakAudio` selectors.
3. MediaCore Host supports 32-bit floating point planar audio buffer exchange.

#### 1.1.2 Explicit Postconditions
1. Premiere Pro does not write `.cfa` or `.pek` auxiliary files to the Media Cache directory upon asset import.
2. Timeline audio waveforms render within $< 100\,\text{ms}$ of timeline drop via `imGetPeakAudio`.
3. Multi-channel audio (up to 32 discrete audio channels) maps accurately to Premiere timeline tracks without channel swizzling or phase cancellation.
4. Audio-video synchronization remains strictly jitter-free ($\le 1\,\text{sample}$ clock skew) across 4-hour playback sessions.

---

### 1.2 Gherkin Acceptance Criteria

```gherkin
Feature: Zero-Conform Audio Decoding and Peak Waveform Generation

  Scenario: Instant import without audio conforming
    Given an MKV file with 48kHz Opus stereo audio
    When the user drops the file into Premiere Pro Project Panel
    Then the plugin flags avoidAudioConform = kPrTrue in imGetInfo8
    And no .cfa or .pek cache files are generated on disk
    And the audio is immediately playable on the timeline.

  Scenario: Direct peak waveform calculation
    Given Premiere Pro requests audio waveform display for an imported 60-minute video
    When imGetPeakAudio is invoked with sample range and decimation level
    Then PeakWaveformGenerator calculates min and max sample amplitudes directly from demuxed packets
    And returns peak pyramids to Premiere in under 50ms per timeline view quadrant.

  Scenario: Complex multi-channel layout routing
    Given an OBS MKV recording with 6 separate audio tracks (Microphone, Game, Discord, Music)
    When the file is placed on a multi-track sequence
    Then MultiChannelLayoutRouter maps discrete audio channels to independent timeline tracks
    And maintains exact 32-bit float planar sample boundaries without crosstalk.

  Scenario: Sample rate conversion to sequence clock
    Given a 44.1kHz FLAC audio track placed into a 48.0kHz Premiere Pro sequence
    When imImportAudio requests audio buffers at 48.0kHz
    Then AudioResampleConverter uses libswresample to perform sinc-interpolated resampling
    And output signal maintains SNR > 96dB with zero audible aliasing or crackling.
```

---

### 1.3 Test Cases & Traceability Matrix

| Test ID | Category | Description | Covered Unit | Expected Result |
| :--- | :--- | :--- | :--- | :--- |
| `TC-2.4-01` | Audio Decoding | Opus 48kHz to 32-bit float planar PCM | `Unit-1` | Lossless floating point conversion with full dynamic range $[-1.0, 1.0]$ |
| `TC-2.4-02` | Host Protocol | `avoidAudioConform` flag verification in `imGetInfo8` | `Unit-2` | Host sets conform status to bypassed; zero `.cfa` files created |
| `TC-2.4-03` | Waveforms | Peak audio pyramid generation accuracy | `Unit-3` | Min/max values match baseline uncompressed PCM analysis within $0.001\%$ |
| `TC-2.4-04` | Channel Matrix | 5.1 surround to 6-channel planar mapping | `Unit-4` | Correct order: L, R, C, LFE, Ls, Rs; no channel swapping |
| `TC-2.4-05` | Resampling | Resampling 44.1kHz / 96kHz to 48kHz timeline clock | `Unit-5` | Zero phase drift; sample count aligns with sequence frame rate |

---

## 2. Technical Implementation Plan

### 2.1 Domain Invariants, Data Models & Storage Schema

```cpp
namespace OpenMedia::Importer::Audio {

struct AudioFormatDescriptor {
    AVCodecID codecId;
    int sampleRate;
    int channels;
    uint64_t channelLayout;
    AVSampleFormat sampleFormat;
    bool isPlanar;
};

struct PeakPyramidLevel {
    uint32_t decimationFactor; // e.g., 256, 1024, 4096
    std::vector<float> minPeaks;
    std::vector<float> maxPeaks;
};

struct AudioPacketChunk {
    int64_t startSample;
    int64_t sampleCount;
    std::vector<float*> planarBuffers; // [channel][sample]
};

} // namespace OpenMedia::Importer::Audio
```

### 2.2 Activity Diagram: Audio Import & Waveform Request Cycle

```mermaid
flowchart TD
    A[Premiere Pro calls imGetInfo8] --> B[Plugin queries AudioFormatDescriptor]
    B --> C[Set avoidAudioConform = kPrTrue]
    C --> D[Set accessModes |= kRandomAccessImport]
    D --> E[Premiere Pro draws timeline track]
    E --> F{Host calls selector?}
    F -->|imGetPeakAudio| G[PeakWaveformGenerator: direct packet min/max analysis]
    G --> H[Return peak buffer to Host without disk I/O]
    F -->|imImportAudio| I[ZeroConformAudioExtractor: decode Opus/FLAC frame]
    I --> J{Sample rate == Sequence rate?}
    J -->|No| K[AudioResampleConverter: libswresample]
    J -->|Yes| L[MultiChannelLayoutRouter: planar routing]
    K --> L
    L --> M[Write to Host 32-bit float audio buffer]
```

---

## 3. Expected Folder Architecture at the Project

```
plugins/importer/
├── include/
│   ├── OpenMedia_AudioExtractor.h
│   ├── OpenMedia_AudioConformBypass.h
│   ├── OpenMedia_PeakWaveformGenerator.h
│   ├── OpenMedia_MultiChannelRouter.h
│   └── OpenMedia_AudioResampler.h
├── src/
│   ├── OpenMedia_AudioExtractor.cpp
│   ├── OpenMedia_AudioConformBypass.cpp
│   ├── OpenMedia_PeakWaveformGenerator.cpp
│   ├── OpenMedia_MultiChannelRouter.cpp
│   └── OpenMedia_AudioResampler.cpp
└── tests/
    ├── Test_AudioExtractor.cpp
    ├── Test_AudioConformBypass.cpp
    ├── Test_PeakWaveformGenerator.cpp
    ├── Test_MultiChannelRouter.cpp
    └── Test_AudioResampler.cpp
```

---

## 4. Implementation Units in Detail & Specialist Agent Assignment

### 4.1 Unit Sizing Decision Matrix & Score Calculation

$$S_{\text{story}} = 0.35 D_1 + 0.25 D_2 + 0.20 D_3 + 0.20 D_4$$
- $D_1 = 3$ (Audio decoders, peak wave, channel mapping)
- $D_2 = 3$ (~950 LOC across audio subsystem)
- $D_3 = 3$ (Sample rate conversion, floating point math, channel layout matrix)
- $D_4 = 2$ (Audio decoder, peak extractor, resampler)
$$S_{\text{story}} = (0.35 \times 3) + (0.25 \times 3) + (0.20 \times 3) + (0.20 \times 2) = 1.05 + 0.75 + 0.60 + 0.40 = 2.80 \implies K = 5$$

### 4.2 Dynamic $K$ Implementation Unit Specifications

#### Unit 1: Zero-Conform Planar Audio Extractor
- **ID**: `unit-1`
- **Branch**: `feat/us-2.4.0/unit-1-audio-extractor`
- **Objective**: Decoding pipeline for Opus, FLAC, Vorbis, and PCM streams yielding IEEE 754 32-bit planar floating-point buffers.
- **Files Owned**:
  - `plugins/importer/include/OpenMedia_AudioExtractor.h`
  - `plugins/importer/src/OpenMedia_AudioExtractor.cpp`
  - `plugins/importer/tests/Test_AudioExtractor.cpp`
- **Verification**: `ctest -R Test_AudioExtractor` verifies Opus/FLAC frame decoding into planar float arrays with 0 sample clipping.

#### Unit 2: Audio Conform Bypass Selector Protocol
- **ID**: `unit-2`
- **Branch**: `feat/us-2.4.0/unit-2-audio-conform-bypass`
- **Objective**: Implements MediaCore `imGetInfo8` / `imGetIndAudioInfo` flags `avoidAudioConform` and random access audio flags.
- **Files Owned**:
  - `plugins/importer/include/OpenMedia_AudioConformBypass.h`
  - `plugins/importer/src/OpenMedia_AudioConformBypass.cpp`
  - `plugins/importer/tests/Test_AudioConformBypass.cpp`
- **Verification**: `ctest -R Test_AudioConformBypass` mocks host `imGetInfo8` struct and verifies correct bitmask flags are set.

#### Unit 3: Instantaneous Peak Waveform Generator
- **ID**: `unit-3`
- **Branch**: `feat/us-2.4.0/unit-3-peak-waveform-generator`
- **Objective**: Handles `imGetPeakAudio` selector, synthesizing multi-resolution peak pyramids directly from demuxed packets in memory.
- **Files Owned**:
  - `plugins/importer/include/OpenMedia_PeakWaveformGenerator.h`
  - `plugins/importer/src/OpenMedia_PeakWaveformGenerator.cpp`
  - `plugins/importer/tests/Test_PeakWaveformGenerator.cpp`
- **Verification**: `ctest -R Test_PeakWaveformGenerator` verifies peak min/max bounds and timing performance $< 50\,\text{ms}$.

#### Unit 4: Multi-Channel Layout Matrix Router
- **ID**: `unit-4`
- **Branch**: `feat/us-2.4.0/unit-4-multi-channel-router`
- **Objective**: Channel routing and remapping engine supporting standard configurations (Mono, Stereo, 5.1, 7.1) and up to 32 discrete tracks.
- **Files Owned**:
  - `plugins/importer/include/OpenMedia_MultiChannelRouter.h`
  - `plugins/importer/src/OpenMedia_MultiChannelRouter.cpp`
  - `plugins/importer/tests/Test_MultiChannelRouter.cpp`
- **Verification**: `ctest -R Test_MultiChannelRouter` verifies 5.1/7.1 channel matrix conversion and discrete channel mapping.

#### Unit 5: High-Precision Audio Resampler (libswresample)
- **ID**: `unit-5`
- **Branch**: `feat/us-2.4.0/unit-5-audio-resampler`
- **Objective**: High-fidelity polyphase sinc audio resampler using FFmpeg `libswresample` to reconcile source and timeline clock mismatches.
- **Files Owned**:
  - `plugins/importer/include/OpenMedia_AudioResampler.h`
  - `plugins/importer/src/OpenMedia_AudioResampler.cpp`
  - `plugins/importer/tests/Test_AudioResampler.cpp`
- **Verification**: `ctest -R Test_AudioResampler` tests 44.1kHz $\to$ 48kHz conversion, measuring SNR and frequency response.

### 4.3 Specialist Agent Assignment

| Unit | Subagent Type | Specialist Role |
| :--- | :--- | :--- |
| `unit-1` | `agency-voice-ai-integration-engineer` | Audio Codec & DSP Specialist |
| `unit-2` | `agency-minimal-change-engineer` | MediaCore SDK Protocol Specialist |
| `unit-3` | `agency-data-visualization-engineer` | Waveform Pyramid & Decimation Specialist |
| `unit-4` | `agency-backend-architect` | Multi-Channel Matrix & DSP Architect |
| `unit-5` | `agency-audio-processing-specialist` | Audio Resampling & libswresample Specialist |

### 4.4 Shared Base Contract & Seeding Plan
- `AudioFormatDescriptor` and `PeakPyramidLevel` declared in `OpenMedia_AudioTypes.h`.
- Resampler and Extractor expose clean C++ interfaces with no direct Premiere SDK dependencies.

---

## 5. Security Gateway powered by `security-audit`

### 5.1 Trust Boundaries & Actors
- **Untrusted Input**: Corrupted audio packets with invalid sample rates (e.g., negative sample rates or divide-by-zero channels).
- **Audio Output**: Direct memory buffers provided by Premiere Pro host.

### 5.2 STRIDE Threat Matrix & ASVS 5.0 Controls

| Threat Category | Scenario | Mitigation / ASVS 5.0 Control |
| :--- | :--- | :--- |
| **Denial of Service** | Audio packet claims $10^9$ samples causing heap exhaustion | Strict sample batch chunk ceiling ($65,536$ samples max per call) |
| **Tampering** | Integer overflow in byte size calculation for multi-channel buffers | Safe math with `std::overflow_error` guard (ASVS V5.1.1) |
| **Information Disclosure** | Audio buffer underflow reading adjacent heap memory | Buffer zeroing and strict clamping to decoded sample count |

### 5.3 Global Cryptographic, Jurisdictional & Data Protection Controls
- Audio stream metadata sanitization guarantees zero leakage of embedded personal identifiers or recording device metadata.

### 5.4 Automated Security Verification Checklist
- [x] Fuzz testing with corrupted Opus/FLAC packets triggers clean error return without segfault.
- [x] Buffer bounds checking verified across all 32 channels.
- [x] Arithmetic overflow check on `sampleCount * channels * sizeof(float)`.

### 5.5 Gate G2 Sign-Off Checklist
- [x] No unsafe raw pointer arithmetic in audio buffer indexing.
- [x] Zero memory leaks verified by Valgrind / ASAN on audio stream teardown.

---

## 6. Validation Gates & Acceptance Sign-Off

| Gate | Criterion | Verification Command | Pass Condition |
| :--- | :--- | :--- | :--- |
| **Gate G0** | Compilation & Formatter | `cmake --build --preset windows-x64-release` | 0 errors, 0 warnings |
| **Gate G1** | Verification Coverage | `ctest -R Test_Audio*` | 100% tests pass; coverage $\ge 90\%$ |
| **Gate G2** | Security Audit | `scripts/security-audit-scan.sh --target audio` | 0 high/critical issues |
| **Gate G-Complexity** | Cyclomatic Complexity | `python scripts/check-complexity.py plugins/importer/audio` | $CC \le 6$ target, $CC \le 10$ max ceiling |
| **Gate G-Docs** | API Documentation | Doxygen comment check | 100% audio functions documented |
| **Gate G-Review** | Independent Council | GitHub PR Conversation Sign-Off | Approval comment from Code Reviewer Council |
