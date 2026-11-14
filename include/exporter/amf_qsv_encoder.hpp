#pragma once
/// @file amf_qsv_encoder.hpp
/// @brief AMD AMF and Intel QSV/VA-API AV1 hardware encoder wrapper.
#include "openmedia/core_types.hpp"
namespace openmedia::exporter {
/// @brief Vendor selection for AMD/Intel hardware AV1 encoding.
enum class HwVendor { AMD, IntelQSV, IntelVAAPI };
/// @brief Selects vendor encoder and NV12/P010LE surface pixel format.
class AmfQsvEncoder {
public:
    static const char* encoderName(HwVendor vendor) noexcept;
    static PixelFormat surfaceFormat(int32_t bitDepth) noexcept;
};
} // namespace openmedia::exporter
