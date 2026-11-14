#include "exporter/gop_tail_flusher.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    uint32_t drained = 0;
    bool trailer = false;
    assert(GopTailFlusher::flushAndFinalize(16, drained, trailer) == kExNoErr);
    assert(drained == 16 && trailer);
    std::cout << "[PASS] TC-3.2.5 GopTailFlusher drain & trailer finalization passed.\n";
    return 0;
}
