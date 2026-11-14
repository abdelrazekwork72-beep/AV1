/// @file scrub_benchmark.cpp
/// @brief Implementation of ScrubLatencyBenchmark enforcing imGetSourceVideo and 2000ms watchdog.

#include "scrub_benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <numeric>
#include <random>

namespace openmedia::integration {

namespace {

bool isAligned16(const void* ptr) noexcept {
    if (!ptr) {
        return true;
    }
    return (reinterpret_cast<std::uintptr_t>(ptr) % 16U) == 0U;
}

double measureSeekBatch(const std::vector<std::int64_t>& schedule,
                        bool useCache,
                        const AsyncSourceVideoFn& fetchFrame,
                        std::vector<double>& outSamplesMs,
                        bool& outAligned,
                        bool& outTimedOut) {
    outSamplesMs.clear();
    outSamplesMs.reserve(schedule.size());
    for (std::int64_t frame : schedule) {
        void* ppix = nullptr;
        const auto t0 = std::chrono::steady_clock::now();
        const bool ok = fetchFrame(frame, useCache, &ppix);
        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (!ok || ms > static_cast<double>(ScrubLatencyBenchmark::kWatchdogTimeoutMs)) {
            outTimedOut = true;
            return 0.0;
        }
        if (!isAligned16(ppix)) {
            outAligned = false;
        }
        outSamplesMs.push_back(ms);
    }
    std::sort(outSamplesMs.begin(), outSamplesMs.end());
    return ScrubLatencyBenchmark::computePercentile(outSamplesMs, 0.95);
}

} // namespace

std::vector<std::int64_t> ScrubLatencyBenchmark::buildSeekSchedule(
    std::int64_t totalFrames,
    std::size_t seekCount) {
    std::vector<std::int64_t> schedule;
    if (totalFrames <= 0 || seekCount == 0) {
        return schedule;
    }
    schedule.reserve(seekCount);
    std::mt19937_64 rng(42ULL);
    std::uniform_int_distribution<std::int64_t> dist(0, totalFrames - 1);

    for (std::size_t i = 0; i < seekCount; ++i) {
        const std::size_t phase = i % 3U;
        if (phase == 0U) {
            schedule.push_back(static_cast<std::int64_t>(i) % totalFrames);
        } else if (phase == 1U) {
            schedule.push_back(dist(rng));
        } else {
            schedule.push_back(totalFrames - 1 - (static_cast<std::int64_t>(i) % totalFrames));
        }
    }
    return schedule;
}

double ScrubLatencyBenchmark::computePercentile(
    const std::vector<double>& sortedSamplesMs,
    double quantile) noexcept {
    if (sortedSamplesMs.empty()) {
        return 0.0;
    }
    const double clamped = std::clamp(quantile, 0.0, 1.0);
    const std::size_t idx = static_cast<std::size_t>(
        clamped * static_cast<double>(sortedSamplesMs.size() - 1));
    return sortedSamplesMs[idx];
}

ScrubBenchmarkReport ScrubLatencyBenchmark::runScrubBenchmark(
    std::int64_t totalFrames,
    std::size_t seekCount,
    const AsyncSourceVideoFn& fetchFrame) const {
    ScrubBenchmarkReport report{};
    if (!fetchFrame || totalFrames <= 0 || seekCount == 0) {
        return report;
    }
    const auto schedule = buildSeekSchedule(totalFrames, seekCount);
    report.seekCount = schedule.size();

    std::vector<double> uncachedSamples;
    std::vector<double> cachedSamples;
    report.uncachedP95Ms = measureSeekBatch(
        schedule, false, fetchFrame, uncachedSamples,
        report.allFrames16ByteAligned, report.watchdogTimedOut);
    if (report.watchdogTimedOut) {
        return report;
    }

    report.p95Ms = measureSeekBatch(
        schedule, true, fetchFrame, cachedSamples,
        report.allFrames16ByteAligned, report.watchdogTimedOut);
    report.p50Ms = computePercentile(cachedSamples, 0.50);
    report.p99Ms = computePercentile(cachedSamples, 0.99);

    if (report.uncachedP95Ms > 0.0) {
        report.ringBufferSpeedupPct =
            ((report.uncachedP95Ms - report.p95Ms) / report.uncachedP95Ms) * 100.0;
    }
    return report;
}

} // namespace openmedia::integration
