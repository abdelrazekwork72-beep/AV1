#include "exporter/push_render_loop.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    std::vector<openmedia::RawVideoFrame> frames;
    assert(PushRenderLoop::runMultiPass(2, 5, 1921, 16, frames) == kExNoErr);
    assert(frames.size() == 5 && frames[0].rowBytes[0] % 16U == 0U);

    std::vector<openmedia::RawVideoFrame> drainedFrames;
    uint32_t drainedCount = 0;
    bool trailerWritten = false;
    assert(PushRenderLoop::runMultiPassWithDrain(2, 10, 1920, 1080, 1000, drainedFrames, drainedCount, trailerWritten) == kExNoErr);
    assert(drainedFrames.size() == 10);
    assert(drainedFrames.front().pts == 0);
    assert(drainedFrames.back().pts == 9);
    assert(drainedCount == 1 && trailerWritten);

    std::cout << "[PASS] TC-3.2.1 PushRenderLoop 2-pass copy, zero-PTS, and final-frame EOF drain passed.\n";
    return 0;
}
