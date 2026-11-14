#include "importer/ppix_cache_bridge.hpp"

#include <cassert>
#include <chrono>
#include <iostream>

using namespace openmedia::importer;

int main() {
    PPixCacheBridge bridge;
    int dummyPixelBuf = 42;
    PPixHand handle = &dummyPixelBuf;

    PPixHand out = nullptr;
    assert(!bridge.getFrame(1, 42, out));
    assert(bridge.missCount() == 1);

    assert(bridge.addFrame(1, 42, handle) == kImNoErr);

    const auto t0 = std::chrono::steady_clock::now();
    assert(bridge.getFrame(1, 42, out));
    const auto t1 = std::chrono::steady_clock::now();
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    assert(out == handle);
    assert(bridge.hitCount() == 1);
    assert(us < 1000);

    std::cout << "[PASS] TC-2.3.2 PPixCacheBridge frame 42 hit in " << us << " us.\n";
    return 0;
}
