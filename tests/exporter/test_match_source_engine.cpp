#include "exporter/match_source_engine.hpp"

#include <cassert>
#include <iostream>

using namespace openmedia::exporter;

int main() {
    SequenceMatchProfile seq{3840, 2160, 24000, 1001, 0, 1.0};
    SequenceMatchProfile out{1280, 720, 30, 1, 1, 1.0};
    assert(MatchSourceEngine::conformToSequence(true, seq, out) == kExNoErr);
    assert(out.width == 3840 && out.height == 2160 && out.fpsNum == 24000 && out.fpsDen == 1001);

    std::cout << "[PASS] TC-3.1.3 MatchSourceEngine sequence sync passed.\n";
    return 0;
}
