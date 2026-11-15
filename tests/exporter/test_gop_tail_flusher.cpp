#include "exporter/gop_tail_flusher.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace openmedia::exporter;

int main() {
    uint32_t drained = 0;
    bool trailer = false;
    assert(GopTailFlusher::flushAndFinalize(16, drained, trailer) == kExNoErr);
    assert(drained == 16 && trailer);

    std::vector<openmedia::RawVideoFrame> pendingTail(2);
    pendingTail[0].pts = 98;
    pendingTail[1].pts = 99;
    std::vector<openmedia::RawVideoFrame> outFrames;
    bool eofTrailer = false;
    assert(GopTailFlusher::drainAtEof(pendingTail, outFrames, eofTrailer) == kExNoErr);
    assert(pendingTail.empty());
    assert(outFrames.size() == 2 && outFrames[1].pts == 99 && eofTrailer);

    std::cout << "[PASS] TC-3.2.5 GopTailFlusher drain & final-frame EOF retention passed.\n";
    return 0;
}
