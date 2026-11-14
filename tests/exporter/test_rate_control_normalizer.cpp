#include "exporter/rate_control_normalizer.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    assert(RateControlNormalizer::toAv1BaseQIndex(0) == 0 && RateControlNormalizer::toNvencCq(0) == 0);
    assert(RateControlNormalizer::toAv1BaseQIndex(18) == 73 && RateControlNormalizer::toNvencCq(18) == 15);
    assert(RateControlNormalizer::toAv1BaseQIndex(24) == 97 && RateControlNormalizer::toNvencCq(24) == 19);
    assert(RateControlNormalizer::toAv1BaseQIndex(32) == 130 && RateControlNormalizer::toNvencCq(32) == 26);
    assert(RateControlNormalizer::toAv1BaseQIndex(63) == 255 && RateControlNormalizer::toNvencCq(63) == 51);
    assert(RateControlNormalizer::toVp9Crf(24) == 24);
    std::cout << "[PASS] TC-3.3.5 RateControlNormalizer CRF/QP/base_q_index (0, 18, 24, 32, 63) passed.\n";
    return 0;
}
