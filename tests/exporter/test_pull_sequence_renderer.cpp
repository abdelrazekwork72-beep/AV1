#include "exporter/pull_sequence_renderer.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    openmedia::RawVideoFrame frame;
    assert(PullSequenceRenderer::renderFrame(12, 1920, 1080, openmedia::PixelFormat::BGRA8, frame) == kExNoErr);
    assert(frame.pts == 12 && frame.width == 1920 && frame.height == 1080);
    std::cout << "[PASS] TC-3.2.2 PullSequenceRenderer frame conforming passed.\n";
    return 0;
}
