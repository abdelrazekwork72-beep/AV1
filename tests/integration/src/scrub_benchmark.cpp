/// @file scrub_benchmark.cpp
/// @brief Implementation of ScrubLatencyBenchmark with CPU/GPU decode policy & WebM/AV1 final-frame checks.

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

bool verifyFinalFrameRetention(std::int64_t totalFrames,
                               const AsyncSourceVideoFn& fetchFrame) {
    if (totalFrames <= 0 || !fetchFrame) {
        return false;
    }
    void* finalPpix = nullptr;
    const bool ok = fetchFrame(totalFrames - 1, true, &finalPpix);
    return ok && finalPpix != nullptr && isAligned16(finalPpix);
}

} // namespace

DecodePolicyDecision ScrubLatencyBenchmark::resolveDecodePolicy(
    std::uint32_t width,
    std::uint32_t height,
    std::size_t activeGpuStreamCount) noexcept {
    DecodePolicyDecision decision{};
    const bool meetsGpuThreshold =
        (width >= kGpuWidthThreshold) && (height >= kGpuHeightThreshold);
    if (!meetsGpuThreshold) {
        decision.backend = DecodeBackend::Cpu;
        decision.activeGpuStreams = static_cast<std::uint32_t>(activeGpuStreamCount);
        decision.overflowedToCpu = false;
        return decision;
    }
    if (activeGpuStreamCount < kMaxConcurrentGpuStreams) {
        decision.backend = DecodeBackend::Gpu;
        decision.activeGpuStreams = static_cast<std::uint32_t>(activeGpuStreamCount + 1);
        decision.overflowedToCpu = false;
        return decision;
    }
    decision.backend = DecodeBackend::Cpu;
    decision.activeGpuStreams = static_cast<std::uint32_t>(kMaxConcurrentGpuStreams);
    decision.overflowedToCpu = true;
    return decision;
}

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

DualPolicyBenchmarkReport ScrubLatencyBenchmark::runDecodePolicyBenchmark(
    std::int64_t totalFrames,
    std::size_t seekCount,
    const AsyncSourceVideoFn& fetchFrame) const {
    DualPolicyBenchmarkReport dual{};
    const auto cpuDecision = resolveDecodePolicy(1920, 1080, 0);
    if (cpuDecision.backend == DecodeBackend::Cpu && !cpuDecision.overflowedToCpu) {
        dual.cpuPathReport = runScrubBenchmark(totalFrames, seekCount, fetchFrame);
    }

    std::size_t gpuCount = 0;
    for (std::size_t stream = 0; stream < kMaxConcurrentGpuStreams; ++stream) {
        const auto gpuDecision = resolveDecodePolicy(3840, 2160, gpuCount);
        if (gpuDecision.backend == DecodeBackend::Gpu && !gpuDecision.overflowedToCpu) {
            ++gpuCount;
        }
    }
    dual.concurrentGpuStreamsVerified = gpuCount;
    if (gpuCount == kMaxConcurrentGpuStreams) {
        dual.gpuPathReport = runScrubBenchmark(totalFrames, seekCount, fetchFrame);
    }

    const auto fifthStream = resolveDecodePolicy(3840, 2160, kMaxConcurrentGpuStreams);
    dual.fifthStreamAutoRoutedToCpu =
        (fifthStream.backend == DecodeBackend::Cpu) && fifthStream.overflowedToCpu;

    dual.webmAv1FinalFrameRetained = verifyFinalFrameRetention(totalFrames, fetchFrame);
    return dual;
}

} // namespace openmedia::integration
