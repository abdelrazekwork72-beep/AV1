/// @file test_scrub_benchmark.cpp
/// @brief Standalone C++17 benchmark test for TC-4.1.2 (Dual-Policy ScrubLatencyBenchmark).

#undef NDEBUG
#include "scrub_benchmark.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
    alignas(16) std::uint8_t alignedFrameBuffer[64]{};
    openmedia::integration::ScrubLatencyBenchmark bench;

    // 1. Verify Decode Policy Routing (<2560x1600 CPU, >=2560x1600 GPU up to 4 streams, 5th -> CPU)
    const auto hdPolicy = openmedia::integration::ScrubLatencyBenchmark::resolveDecodePolicy(1920, 1080, 0);
    assert(hdPolicy.backend == openmedia::integration::DecodeBackend::Cpu);
    assert(!hdPolicy.overflowedToCpu);

    const auto uhdPolicy4 = openmedia::integration::ScrubLatencyBenchmark::resolveDecodePolicy(3840, 2160, 3);
    assert(uhdPolicy4.backend == openmedia::integration::DecodeBackend::Gpu);
    assert(uhdPolicy4.activeGpuStreams == 4);
    assert(!uhdPolicy4.overflowedToCpu);

    const auto uhdPolicy5 = openmedia::integration::ScrubLatencyBenchmark::resolveDecodePolicy(3840, 2160, 4);
    assert(uhdPolicy5.backend == openmedia::integration::DecodeBackend::Cpu);
    assert(uhdPolicy5.overflowedToCpu);

    // 2. Simulate AsyncImporterEntry(imGetSourceVideo) with GOP RingBuffer cache speedup & final frame retention
    const auto fetchFn = [&alignedFrameBuffer](std::int64_t /*frameIndex*/,
                                               bool useRingBufferCache,
                                               void** outAlignedPPix) -> bool {
        const auto delayUs = std::chrono::microseconds(useRingBufferCache ? 300 : 500);
        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < delayUs) {
        }
        if (outAlignedPPix) {
            *outAlignedPPix = alignedFrameBuffer;
        }
        return true;
    };

    const auto report = bench.runScrubBenchmark(3600, 200, fetchFn);
    assert(report.meetsSla());

    const auto dualReport = bench.runDecodePolicyBenchmark(3600, 200, fetchFn);
    assert(dualReport.passed());
    assert(dualReport.cpuPathReport.p95Ms < 50.0);
    assert(dualReport.gpuPathReport.p95Ms < 50.0);
    assert(dualReport.concurrentGpuStreamsVerified == 4);
    assert(dualReport.fifthStreamAutoRoutedToCpu);
    assert(dualReport.webmAv1FinalFrameRetained);

    std::cout << "[PASS] TC-4.1.2 Dual-Policy ScrubLatencyBenchmark: CPU p95="
              << dualReport.cpuPathReport.p95Ms << "ms, GPU p95="
              << dualReport.gpuPathReport.p95Ms << "ms, 5th-stream CPU overflow & 100% final-frame retention verified.\n";
    return 0;
}
