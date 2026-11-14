/// @file test_scrub_benchmark.cpp
/// @brief Standalone C++17 benchmark test for TC-4.1.2 (ScrubLatencyBenchmark).

#include "scrub_benchmark.hpp"

#include <cassert>
#include <iostream>

int main() {
    openmedia::integration::ScrubLatencyBenchmark bench;

    // 1. Default 200-seek pipeline benchmark exercising AsyncImporterEngine, KeyframeRingBuffer, and PPixCacheBridge
    const auto report = bench.runDefaultPipelineBenchmark(200);
    assert(report.seekCount == 200);
    assert(report.asyncSelectorVerified);
    assert(report.p50Ms < 25.0);
    assert(report.p95Ms < 50.0);
    assert(report.ringBufferSpeedupPct >= 5.0);
    assert(report.allFrames16ByteAligned);
    assert(!report.watchdogTimedOut);
    assert(report.meetsSla());

    // 2. Watchdog failure / error propagation check (TM-4.1.2)
    const auto failReport = bench.runScrubBenchmark(
        100, 10, [](std::int64_t, bool, void**) { return false; });
    assert(failReport.watchdogTimedOut);
    assert(!failReport.meetsSla());

    std::cout << "[PASS] TC-4.1.2 ScrubLatencyBenchmark: seeks=" << report.seekCount
              << ", p50=" << report.p50Ms << "ms, p95=" << report.p95Ms
              << "ms, speedup=" << report.ringBufferSpeedupPct << "%" << std::endl;
    return 0;
}
