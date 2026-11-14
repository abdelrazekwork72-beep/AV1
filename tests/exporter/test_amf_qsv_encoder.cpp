#include "exporter/amf_qsv_encoder.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    assert(std::strcmp(AmfQsvEncoder::encoderName(HwVendor::AMD), "av1_amf") == 0);
    assert(std::strcmp(AmfQsvEncoder::encoderName(HwVendor::IntelQSV), "av1_qsv") == 0);
    assert(AmfQsvEncoder::surfaceFormat(10) == openmedia::PixelFormat::P010LE);
    std::cout << "[PASS] TC-3.3.3 AmfQsvEncoder NV12/P010LE selection passed.\n";
    return 0;
}
