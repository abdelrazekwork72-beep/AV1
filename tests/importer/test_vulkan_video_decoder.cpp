#include "importer/vulkan_video_decoder.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace openmedia::importer;

int main() {
    VulkanVideoDecoder dec;
    assert(VulkanVideoDecoder::supportsCodec(openmedia::CodecId::AV1));
    assert(VulkanVideoDecoder::supportsCodec(openmedia::CodecId::VP9));

    assert(dec.openSession(openmedia::CodecId::AV1, true));
    assert(dec.isHardwareActive());
    assert(std::strcmp(dec.backendName(), "vulkan") == 0);

    assert(dec.openSession(openmedia::CodecId::VP9, false));
    assert(!dec.isHardwareActive());
    assert(std::strcmp(dec.backendName(), "cpu_fallback") == 0);

    std::cout << "[PASS] TC-2.3.6 VulkanVideoDecoder AV1/VP9 & CPU fallback passed.\n";
    return 0;
}
