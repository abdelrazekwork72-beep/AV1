#pragma once
/// @file scrub_benchmark.hpp
/// @brief Real-time timeline scrubbing benchmark via imCreateAsyncImporter -> AsyncImporterEntry(imGetSourceVideo).

#include "mock_premiere_suites.hpp"
#include "importer/async_importer.hpp"
#include "importer/keyframe_ring_buffer.hpp"
#include "importer/ppix_cache_bridge.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace openmedia::integration {

/// @brief Callback invoked per frame request via imCreateAsyncImporter -> AsyncImporterEntry(imGetSourceVideo).
using AsyncSourceVideoFn = std::function<bool(std::int64_t frameIndex,
                                              bool useRingBufferCache,
                                              void** outAlignedPPix)>;

/// @brief Summary metrics produced by ScrubLatencyBenchmark (US-4.1.0 Unit 2 / AC-2 / TC-4.1.2).
struct ScrubBenchmarkReport {
    std::size_t seekCount{0};
    double p50Ms{0.0};
    double p95Ms{0.0};
    double p99Ms{0.0};
    double uncachedP95Ms{0.0};
    double ringBufferSpeedupPct{0.0};
    bool asyncSelectorVerified{false};
    bool allFrames16ByteAligned{true};
    bool watchdogTimedOut{false};

    /// @brief Returns true if p50 < 25ms, p95 < 50ms, speedup in [5%, 20%] (or >= 5%), and no timeout occurred.
    [[nodiscard]] bool meetsSla() const noexcept {
        return seekCount >= 200 && p50Ms < 25.0 && p95Ms < 50.0 &&
               ringBufferSpeedupPct >= 5.0 && asyncSelectorVerified &&
               allFrames16ByteAligned && !watchdogTimedOut;
    }
};

/// @brief Benchmarks imGetSourceVideo scrubbing latency and GOP KeyframeRingBuffer + PPixCacheBridge speedup.
class ScrubLatencyBenchmark {
public:
    static constexpr std::int64_t kWatchdogTimeoutMs = 2000;

    /// @brief Runs 200+ seeks through AsyncImporterEngine::handleCreateAsyncImporter -> asyncImporterEntry(kSelGetSourceVideo).
    [[nodiscard]] ScrubBenchmarkReport runScrubBenchmark(
        std::int64_t totalFrames,
        std::size_t seekCount,
        const AsyncSourceVideoFn& fetchFrame) const;

    /// @brief Executes the default end-to-end 4K 10-bit AV1 scrub benchmark exercising real KeyframeRingBuffer and PPixCacheBridge.
    [[nodiscard]] ScrubBenchmarkReport runDefaultPipelineBenchmark(
        std::size_t seekCount = 200) const;

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
