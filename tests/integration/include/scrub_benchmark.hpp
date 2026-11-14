#pragma once
/// @file scrub_benchmark.hpp
/// @brief Real-time timeline scrubbing benchmark via AsyncImporterEntry(imGetSourceVideo).

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

/// @brief Benchmarks imGetSourceVideo scrubbing latency and GOP RingBuffer speedup.
class ScrubLatencyBenchmark {
public:
    static constexpr std::int64_t kWatchdogTimeoutMs = 2000;

    /// @brief Runs 200+ seeks through AsyncImporterEntry(imGetSourceVideo) and computes percentiles.
    /// @param totalFrames Total frames in the 4K 60fps 10-bit AV1/VP9 test stream.
    /// @param seekCount Number of seeks to execute (minimum 200 for SLA).
    /// @param fetchFrame AsyncImporterEntry(imGetSourceVideo) callback.
    /// @return Benchmark report with p50/p95/p99 and RingBuffer speedup.
    [[nodiscard]] ScrubBenchmarkReport runScrubBenchmark(
        std::int64_t totalFrames,
        std::size_t seekCount,
        const AsyncSourceVideoFn& fetchFrame) const;

    /// @brief Generates deterministic sequential, random, and reverse seek indices.
    /// @param totalFrames Total frames available in stream.
    /// @param seekCount Number of seek indices to generate.
    /// @return Vector of frame indices.
    [[nodiscard]] static std::vector<std::int64_t> buildSeekSchedule(
        std::int64_t totalFrames,
        std::size_t seekCount);

    /// @brief Computes percentile (e.g. 0.50, 0.95, 0.99) from sorted latency samples.
    /// @param sortedSamplesMs Sorted latency values in milliseconds.
    /// @param quantile Quantile in [0.0, 1.0].
    /// @return Percentile latency in milliseconds.
    [[nodiscard]] static double computePercentile(
        const std::vector<double>& sortedSamplesMs,
        double quantile) noexcept;
};

} // namespace openmedia::integration
