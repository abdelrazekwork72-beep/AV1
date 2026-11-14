#include "exporter/rate_control_normalizer.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    assert(RateControlNormalizer::toAv1BaseQIndex(63) == 255);
    assert(RateControlNormalizer::toNvencCq(63) == 51);
    assert(RateControlNormalizer::toVp9Crf(32) == 32);
    std::cout << "[PASS] TC-3.3.5 RateControlNormalizer CRF/QP/base_q_index passed.\n";
    return 0;
}
