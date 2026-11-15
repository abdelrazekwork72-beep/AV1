#include "exporter/pull_sequence_renderer.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    openmedia::RawVideoFrame frame;
    assert(PullSequenceRenderer::renderFrame(12, 1920, 1080, openmedia::PixelFormat::BGRA8, frame) == kExNoErr);
    assert(frame.pts == 12 && frame.width == 1920 && frame.height == 1080);

    openmedia::RawVideoFrame normFirst;
    assert(PullSequenceRenderer::renderSequenceFrame(900, 900, 1920, 1080, openmedia::PixelFormat::BGRA8, normFirst) == kExNoErr);
    assert(normFirst.pts == 0);

    openmedia::RawVideoFrame normNext;
    assert(PullSequenceRenderer::renderSequenceFrame(905, 900, 1920, 1080, openmedia::PixelFormat::BGRA8, normNext) == kExNoErr);
    assert(normNext.pts == 5);

    std::cout << "[PASS] TC-3.2.2 PullSequenceRenderer frame conforming & zero-PTS normalization passed.\n";
    return 0;
}
