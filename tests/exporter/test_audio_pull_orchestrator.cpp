#include "exporter/audio_pull_orchestrator.hpp"

#include <cassert>
#include <iostream>

using namespace openmedia::exporter;

int main() {
    std::vector<std::vector<float>> planar = {{0.1f, 0.2f}, {-0.1f, -0.2f}};
    std::vector<float> packed;
    assert(AudioPullOrchestrator::interleavePlanar(planar, packed) == kExNoErr);
    assert(packed.size() == 4 && packed[0] == 0.1f && packed[1] == -0.1f);

    std::cout << "[PASS] TC-3.2.3 AudioPullOrchestrator planar interleave passed.\n";
    return 0;
}
