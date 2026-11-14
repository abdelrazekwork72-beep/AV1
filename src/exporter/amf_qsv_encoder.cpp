#include "exporter/amf_qsv_encoder.hpp"
namespace openmedia::exporter {
const char* AmfQsvEncoder::encoderName(HwVendor vendor) noexcept {
    if (vendor == HwVendor::AMD) return "av1_amf";
    if (vendor == HwVendor::IntelQSV) return "av1_qsv";
    return "av1_vaapi";
}
PixelFormat AmfQsvEncoder::surfaceFormat(int32_t bitDepth) noexcept {
    return (bitDepth >= 10) ? PixelFormat::P010LE : PixelFormat::NV12;
}
} // namespace openmedia::exporter
