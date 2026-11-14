/// @file test_scrub_benchmark.cpp
/// @brief Standalone C++17 benchmark test for TC-4.1.2 (ScrubLatencyBenchmark).

#include "scrub_benchmark.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

int main() {
    alignas(16) std::uint8_t alignedFrameBuffer[64]{};
    openmedia::integration::ScrubLatencyBenchmark bench;

    // Simulate AsyncImporterEntry(imGetSourceVideo) with GOP RingBuffer cache speedup (~15%)
    const auto fetchFn = [&alignedFrameBuffer](std::int64_t /*frameIndex*/,
                                               bool useRingBufferCache,
                                               void** outAlignedPPix) -> bool {
        const auto delayUs = useRingBufferCache ? 800 : 1000;
        std::this_thread::sleep_for(std::chrono::microseconds(delayUs));
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
