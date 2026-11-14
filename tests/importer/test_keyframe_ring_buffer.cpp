#include "importer/keyframe_ring_buffer.hpp"

#include <cassert>
#include <chrono>
#include <iostream>

using namespace openmedia::importer;

int main() {
    KeyframeRingBuffer buf(16);
    // 10,000-frame Long-GOP VFR fixture (keyframe every 120 frames, variable PTS deltas)
    for (int64_t f = 0; f < 10000; f += 120) {
        const int64_t vfrPts = f * 1001 + (f % 360 == 0 ? 15 : 0);
        assert(buf.addKeyframe({vfrPts, f, f * 4096}));
    }

    const auto t0 = std::chrono::steady_clock::now();
    KeyframeEntry kf{};
    for (int64_t q = 0; q < 1000; ++q) {
        assert(buf.findPrecedingKeyframe(q * 10000 + 500, kf));
        assert(kf.pts <= q * 10000 + 500 || kf.frameIndex == 0);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double avgUs =
        std::chrono::duration<double, std::micro>(t1 - t0).count() / 1000.0;
    assert(avgUs < 50.0);

    for (int64_t i = 0; i < 20; ++i) {
        openmedia::RawVideoFrame f;
        f.pts = i * 100;
        f.width = 64;
        f.height = 64;
        buf.pushFrame(f);
    }
    assert(buf.bufferedCount() == 16);
    openmedia::RawVideoFrame hit;
    assert(buf.findBufferedFrame(1900, hit));
    assert(!buf.findBufferedFrame(0, hit));

    std::cout << "[PASS] TC-2.3.3 KeyframeRingBuffer O(log N) avg=" << avgUs << " us.\n";
    return 0;
}
