#pragma once
/// @file scrub_benchmark.hpp
/// @brief Real-time timeline scrubbing & Dual-Policy (CPU/GPU) benchmark via AsyncImporterEntry.

#include "mock_premiere_suites.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace openmedia::integration {

/// @brief Seek pattern for timeline scrubbing benchmarks.
enum class SeekPattern {
    Sequential,
    Random,
    Reverse
};

/// @brief Decode backend selected by Influx-v2 resolution & stream concurrency policy.
enum class DecodeBackend {
    Cpu,
    Gpu
};

/// @brief Result of evaluating decode policy for a given frame resolution and GPU stream count.
struct DecodePolicyDecision {
    DecodeBackend backend{DecodeBackend::Cpu};
    std::uint32_t activeGpuStreams{0};
    bool overflowedToCpu{false};
};

/// @brief Callback invoked per frame request via imCreateAsyncImporter -> AsyncImporterEntry(imGetSourceVideo).
using AsyncSourceVideoFn = std::function<bool(std::int64_t frameIndex,
                                              bool useRingBufferCache,
                                              void** outAlignedPPix)>;

/// @brief Summary metrics produced by ScrubLatencyBenchmark.
struct ScrubBenchmarkReport {
    std::size_t seekCount{0};
    double p50Ms{0.0};
    double p95Ms{0.0};
    double p99Ms{0.0};
    double uncachedP95Ms{0.0};
    double ringBufferSpeedupPct{0.0};
    bool allFrames16ByteAligned{true};
    bool watchdogTimedOut{false};

    /// @brief Returns true if p50 < 25ms, p95 < 50ms, speedup >= 5%, and no timeout occurred.
    [[nodiscard]] bool meetsSla() const noexcept {
        return seekCount >= 200 && p50Ms < 25.0 && p95Ms < 50.0 &&
               ringBufferSpeedupPct >= 5.0 && allFrames16ByteAligned &&
               !watchdogTimedOut;
    }
};

/// @brief Aggregated Dual-Policy benchmark report (<2560x1600 CPU, >=2560x1600 GPU + 5th stream CPU, WebM/AV1 final frame).
struct DualPolicyBenchmarkReport {
    ScrubBenchmarkReport cpuPathReport{};
    ScrubBenchmarkReport gpuPathReport{};
    std::size_t concurrentGpuStreamsVerified{0};
    bool fifthStreamAutoRoutedToCpu{false};
    bool webmAv1FinalFrameRetained{false};

    /// @brief Returns true when CPU/GPU SLAs, 4-stream GPU cap, 5th-stream CPU overflow, and final-frame retention pass.
    [[nodiscard]] bool passed() const noexcept {
        return cpuPathReport.meetsSla() &&
               gpuPathReport.meetsSla() &&
               concurrentGpuStreamsVerified == 4 &&
               fifthStreamAutoRoutedToCpu &&
               webmAv1FinalFrameRetained;
    }
};

/// @brief Benchmarks imGetSourceVideo scrubbing latency, GOP RingBuffer speedup, and CPU/GPU decode policy.
class ScrubLatencyBenchmark {
public:
    static constexpr std::int64_t kWatchdogTimeoutMs = 2000;
    static constexpr std::uint32_t kGpuWidthThreshold = 2560;
    static constexpr std::uint32_t kGpuHeightThreshold = 1600;
    static constexpr std::size_t kMaxConcurrentGpuStreams = 4;

    /// @brief Resolves CPU vs GPU decode policy for resolution and active GPU stream count.
    [[nodiscard]] static DecodePolicyDecision resolveDecodePolicy(
        std::uint32_t width,
        std::uint32_t height,
        std::size_t activeGpuStreamCount) noexcept;

    /// @brief Runs 200+ seeks through AsyncImporterEntry(imGetSourceVideo) and computes percentiles.
    [[nodiscard]] ScrubBenchmarkReport runScrubBenchmark(
        std::int64_t totalFrames,
        std::size_t seekCount,
        const AsyncSourceVideoFn& fetchFrame) const;

    /// @brief Runs CPU (<2560x1600) and GPU (>=2560x1600, 4-stream cap + 5th CPU overflow) + WebM/AV1 final-frame check.
    [[nodiscard]] DualPolicyBenchmarkReport runDecodePolicyBenchmark(
        std::int64_t totalFrames,
        std::size_t seekCount,
        const AsyncSourceVideoFn& fetchFrame) const;

    /// @brief Generates deterministic sequential, random, and reverse seek indices.
    [[nodiscard]] static std::vector<std::int64_t> buildSeekSchedule(
        std::int64_t totalFrames,
        std::size_t seekCount);

    /// @brief Computes percentile (e.g. 0.50, 0.95, 0.99) from sorted latency samples.
    [[nodiscard]] static double computePercentile(
        const std::vector<double>& sortedSamplesMs,
        double quantile) noexcept;
};

} // namespace openmedia::integration
