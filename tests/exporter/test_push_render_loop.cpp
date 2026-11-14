#include "exporter/push_render_loop.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    std::vector<openmedia::RawVideoFrame> frames;
    assert(PushRenderLoop::runMultiPass(2, 5, 1921, 16, frames) == kExNoErr);
    assert(frames.size() == 5 && frames[0].rowBytes[0] % 16U == 0U);
    std::cout << "[PASS] TC-3.2.1 PushRenderLoop 2-pass immediate copy passed.\n";
    return 0;
}
