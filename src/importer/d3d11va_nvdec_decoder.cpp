#include "importer/d3d11va_nvdec_decoder.hpp"

namespace openmedia::importer {

HwBackend D3D11VaNvdecDecoder::initialize(HwBackend preferred, bool simulateHwFailure) noexcept {
    if (simulateHwFailure || preferred == HwBackend::None) {
        active_ = HwBackend::SoftwareFallback;
        return active_;
    }
    active_ = preferred;
    return active_;
}

bool D3D11VaNvdecDecoder::transferAlignedSurface(uint32_t width,
                                                 uint32_t height,
                                                 bool is10Bit,
                                                 std::vector<uint8_t>& outBuffer,
                                                 uint32_t& outStride) const {
    if (width == 0 || height == 0 || width > 16384U || height > 16384U) {
        return false;
    }
    const uint32_t bytesPerSample = is10Bit ? 2U : 1U;
    const uint32_t rawBytes = width * bytesPerSample;
    outStride = ((rawBytes + 15U) / 16U) * 16U;
    const uint32_t totalHeight = height + (height + 1U) / 2U;
    outBuffer.assign(static_cast<size_t>(outStride) * totalHeight, 0U);
    return true;
}

} // namespace openmedia::importer
