#include "exporter/cpu_codecs.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    assert(std::strcmp(CpuCodecBackends::selectEncoderName(openmedia::CodecId::AV1, true), "libsvtav1") == 0);
    assert(std::strcmp(CpuCodecBackends::selectEncoderName(openmedia::CodecId::Opus, true), "libopus") == 0);
    assert(CpuCodecBackends::supportsFormat(openmedia::CodecId::AV1, 10, 444));
    std::cout << "[PASS] TC-3.3.1 CpuCodecBackends software encoders passed.\n";
    return 0;
}
