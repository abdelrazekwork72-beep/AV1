/// @file test_scrub_benchmark.cpp
/// @brief Standalone C++17 benchmark test for TC-4.1.2 (ScrubLatencyBenchmark).

#undef NDEBUG
#include "scrub_benchmark.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
    alignas(16) std::uint8_t alignedFrameBuffer[64]{};
    openmedia::integration::ScrubLatencyBenchmark bench;

    // Simulate AsyncImporterEntry(imGetSourceVideo) with GOP RingBuffer cache speedup (~20%)
    // Uses steady_clock high-resolution busy-wait to avoid Windows 15.6ms timer coalescing.
    const auto fetchFn = [&alignedFrameBuffer](std::int64_t /*frameIndex*/,
                                               bool useRingBufferCache,
                                               void** outAlignedPPix) -> bool {
        const auto delayUs = std::chrono::microseconds(useRingBufferCache ? 400 : 600);
        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < delayUs) {
        }
        if (outAlignedPPix) {
            *outAlignedPPix = alignedFrameBuffer;
        }
        return true;
    };

    const auto report = bench.runScrubBenchmark(3600, 200, fetchFn);
    assert(report.seekCount == 200);
    assert(report.p50Ms < 25.0);
    assert(report.p95Ms < 50.0);
    assert(report.ringBufferSpeedupPct >= 5.0);
    assert(report.allFrames16ByteAligned);
    assert(!report.watchdogTimedOut);
    assert(report.meetsSla());

    std::cout << "[PASS] TC-4.1.2 ScrubLatencyBenchmark: p50=" << report.p50Ms
              << "ms, p95=" << report.p95Ms
              << "ms, speedup=" << report.ringBufferSpeedupPct << "%\n";
    return 0;
}
