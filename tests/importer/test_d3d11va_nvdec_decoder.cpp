#include "importer/d3d11va_nvdec_decoder.hpp"

#include <cassert>
#include <iostream>

using namespace openmedia::importer;

int main() {
    D3D11VaNvdecDecoder dec;
    assert(dec.initialize(HwBackend::D3D11VA, false) == HwBackend::D3D11VA);

    std::vector<uint8_t> buf;
    uint32_t stride = 0;
    assert(dec.transferAlignedSurface(1921, 1080, true, buf, stride));
    assert(stride % 16U == 0U);
    assert(stride >= 1921U * 2U);

    assert(dec.initialize(HwBackend::NVDEC, true) == HwBackend::SoftwareFallback);
    assert(dec.activeBackend() == HwBackend::SoftwareFallback);

    std::cout << "[PASS] TC-2.3.5 D3D11VaNvdecDecoder P010LE alignment & CPU fallback passed.\n";
    return 0;
}
