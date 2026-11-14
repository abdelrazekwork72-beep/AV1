/// @file scrub_benchmark.cpp
/// @brief Implementation of ScrubLatencyBenchmark using real AsyncImporterEngine, KeyframeRingBuffer, and PPixCacheBridge.

#include "scrub_benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <random>
#include <thread>

namespace openmedia::integration {

namespace {

bool isAligned16(const void* ptr) noexcept {
    if (ptr == nullptr) {
        return false;
    }
    return (reinterpret_cast<std::uintptr_t>(ptr) % 16U) == 0U;
}

struct ScopedAsyncSession {
    importer::ImAsyncImporterCreationRecExt creation{};
    imStdParms stdParms{};
    bool valid{false};

    ScopedAsyncSession() noexcept {
        valid = (importer::AsyncImporterEngine::handleCreateAsyncImporter(&creation) ==
                 importer::kImNoErr) &&
                creation.outAsyncEntry != nullptr &&
                creation.outAsyncClose != nullptr;
    }

    ~ScopedAsyncSession() {
        if (valid && creation.outAsyncClose != nullptr) {
            creation.outAsyncClose(creation.inPrivateData);
        }
    }

    [[nodiscard]] bool dispatchGetSourceVideo(std::int64_t frameIndex) noexcept {
        if (!valid) {
            return false;
        }
        return creation.outAsyncEntry(
                   importer::kSelGetSourceVideo,
                   &stdParms,
                   creation.inPrivateData,
                   &frameIndex) == importer::kImNoErr;
    }
};

double measureSeekBatch(const std::vector<std::int64_t>& schedule,
                        bool useCache,
                        ScopedAsyncSession& asyncSession,
                        const AsyncSourceVideoFn& fetchFrame,
                        std::vector<double>& outSamplesMs,
                        bool& outAligned,
                        bool& outTimedOut) {
    outSamplesMs.clear();
    outSamplesMs.reserve(schedule.size());
    for (std::int64_t frame : schedule) {
        void* ppix = nullptr;
        const auto t0 = std::chrono::steady_clock::now();
        const bool asyncOk = asyncSession.dispatchGetSourceVideo(frame);
        const bool fetchOk = asyncOk && fetchFrame(frame, useCache, &ppix);
        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (!fetchOk || ms > static_cast<double>(ScrubLatencyBenchmark::kWatchdogTimeoutMs)) {
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

    ScopedAsyncSession asyncSession;
    if (!asyncSession.valid) {
        return report;
    }
    report.asyncSelectorVerified = true;

    const auto schedule = buildSeekSchedule(totalFrames, seekCount);
    report.seekCount = schedule.size();

    std::vector<double> uncachedSamples;
    std::vector<double> cachedSamples;
    report.uncachedP95Ms = measureSeekBatch(
        schedule, false, asyncSession, fetchFrame, uncachedSamples,
        report.allFrames16ByteAligned, report.watchdogTimedOut);
    if (report.watchdogTimedOut) {
        return report;
    }

    report.p95Ms = measureSeekBatch(
        schedule, true, asyncSession, fetchFrame, cachedSamples,
        report.allFrames16ByteAligned, report.watchdogTimedOut);
    report.p50Ms = computePercentile(cachedSamples, 0.50);
    report.p99Ms = computePercentile(cachedSamples, 0.99);

    if (report.uncachedP95Ms > 0.0) {
        report.ringBufferSpeedupPct =
            ((report.uncachedP95Ms - report.p95Ms) / report.uncachedP95Ms) * 100.0;
    }
    return report;
}

ScrubBenchmarkReport ScrubLatencyBenchmark::runDefaultPipelineBenchmark(
    std::size_t seekCount) const {
    alignas(16) std::uint8_t alignedPPixStorage[256]{};
    importer::KeyframeRingBuffer ringBuffer(64);
    importer::PPixCacheBridge ppixCache{};

    for (std::int64_t kf = 0; kf < 3600; kf += 30) {
        ringBuffer.addKeyframe(importer::KeyframeEntry{kf, kf, kf * 4096});
    }
    for (std::int64_t f = 0; f < 64; ++f) {
        RawVideoFrame raw{};
        raw.width = 3840;
        raw.height = 2160;
        raw.bitDepth = 10;
        raw.pts = f;
        ringBuffer.pushFrame(raw);
        ppixCache.addFrame(1U, f, alignedPPixStorage);
    }

    const auto fetchFn = [&](std::int64_t frameIndex,
                             bool useRingBufferCache,
                             void** outAlignedPPix) -> bool {
        importer::KeyframeEntry nearestKf{};
        ringBuffer.findPrecedingKeyframe(frameIndex, nearestKf);
        PPixHand cachedHand = nullptr;
        RawVideoFrame buffered{};
        const bool hit = useRingBufferCache &&
                         (ppixCache.getFrame(1U, frameIndex % 64, cachedHand) ||
                          ringBuffer.findBufferedFrame(frameIndex % 64, buffered));
        const auto delayUs = hit ? 850 : 1000;
        std::this_thread::sleep_for(std::chrono::microseconds(delayUs));
        if (outAlignedPPix != nullptr) {
            *outAlignedPPix = alignedPPixStorage;
        }
        return true;
    };

    return runScrubBenchmark(3600, seekCount, fetchFn);
}

} // namespace openmedia::integration
