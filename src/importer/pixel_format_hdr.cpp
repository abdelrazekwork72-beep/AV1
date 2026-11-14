/// @file pixel_format_hdr.cpp
/// @brief Implementation of PixelFormatHdrEngine for 10/12-bit HDR negotiation and float conversion.

#include "importer/pixel_format_hdr.hpp"

#include <array>

namespace openmedia::importer {

namespace {

constexpr std::array<HdrFormatDescriptor, PixelFormatHdrEngine::kHdrFormatCount> kHdrFormatTable{{
    {kPixFmtRGB444_10u, "PrPixelFormat_RGB_444_10u", 10U, 3U, false},
    {kPixFmtUYVY422_32f, "PrPixelFormat_UYVY_422_32f", 32U, 3U, true},
    {kPixFmtBGRA4444_32f_Linear, "PrPixelFormat_BGRA_4444_32f_Linear", 32U, 4U, true},
}};

float clampUnitFloat(float value) noexcept {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

bool arePlaneBuffersValid(const std::uint16_t* yPlane,
                          const std::uint16_t* uPlane,
                          const std::uint16_t* vPlane,
                          const float* dstBgra32f) noexcept {
    if (yPlane == nullptr || uPlane == nullptr) {
        return false;
    }
    return (vPlane != nullptr) && (dstBgra32f != nullptr);
}

} // namespace

bool PixelFormatHdrEngine::isHdrBitDepth(std::uint8_t bitDepth) noexcept {
    return (bitDepth == 10U) || (bitDepth == 12U);
}

const HdrFormatDescriptor* PixelFormatHdrEngine::getFormatDescriptor(std::size_t index) noexcept {
    if (index >= kHdrFormatCount) {
        return nullptr;
    }
    return &kHdrFormatTable[index];
}

bool PixelFormatHdrEngine::isSupportedHdrFormat(csSDK_uint32 pixelFormat) noexcept {
    for (std::size_t i = 0U; i < kHdrFormatCount; ++i) {
        if (kHdrFormatTable[i].pixelFormat == pixelFormat) {
            return true;
        }
    }
    return false;
}

csSDK_int32 PixelFormatHdrEngine::negotiateFormat(ImIndPixelFormatRecExt& rec,
                                                  const HdrStreamInfo& streamInfo) noexcept {
    if (!isHdrBitDepth(streamInfo.bitDepth)) {
        return kImUnsupported;
    }
    if (rec.index < 0 || static_cast<std::size_t>(rec.index) >= kHdrFormatCount) {
        return kImBadFormatIndex;
    }
    rec.outPixelFormat = kHdrFormatTable[static_cast<std::size_t>(rec.index)].pixelFormat;
    return kImNoErr;
}

float PixelFormatHdrEngine::normalizeSampleToFloat(std::uint16_t sample,
                                                   std::uint8_t bitDepth) noexcept {
    if (!isHdrBitDepth(bitDepth)) {
        return 0.0f;
    }
    const std::uint32_t maxVal = (1U << bitDepth) - 1U;
    const std::uint32_t raw = static_cast<std::uint32_t>(sample);
    const std::uint32_t clamped = (raw > maxVal) ? maxVal : raw;
    return static_cast<float>(clamped) / static_cast<float>(maxVal);
}

bool PixelFormatHdrEngine::convertYuvSampleToRgb32f(std::uint16_t y,
                                                    std::uint16_t u,
                                                    std::uint16_t v,
                                                    std::uint8_t bitDepth,
                                                    float& outR,
                                                    float& outG,
                                                    float& outB) noexcept {
    if (!isHdrBitDepth(bitDepth)) {
        outR = 0.0f;
        outG = 0.0f;
        outB = 0.0f;
        return false;
    }
    const float yf = normalizeSampleToFloat(y, bitDepth);
    const float uf = normalizeSampleToFloat(u, bitDepth) - 0.5f;
    const float vf = normalizeSampleToFloat(v, bitDepth) - 0.5f;

    // Rec.2020 NCL YCbCr -> normalized RGB without 8-bit quantization.
    outR = clampUnitFloat(yf + 1.4746f * vf);
    outG = clampUnitFloat(yf - 0.16455313f * uf - 0.57135313f * vf);
    outB = clampUnitFloat(yf + 1.8814f * uf);
    return true;
}

bool PixelFormatHdrEngine::convertYuvPlanesToBgra32f(const std::uint16_t* yPlane,
                                                     const std::uint16_t* uPlane,
                                                     const std::uint16_t* vPlane,
                                                     std::size_t pixelCount,
                                                     std::uint8_t bitDepth,
                                                     float* dstBgra32f,
                                                     std::size_t dstFloatCount) noexcept {
    if (!isHdrBitDepth(bitDepth) || dstFloatCount < (pixelCount * 4U)) {
        return false;
    }
    if (!arePlaneBuffersValid(yPlane, uPlane, vPlane, dstBgra32f)) {
        return false;
    }
    for (std::size_t i = 0U; i < pixelCount; ++i) {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        convertYuvSampleToRgb32f(yPlane[i], uPlane[i], vPlane[i], bitDepth, r, g, b);
        const std::size_t base = i * 4U;
        dstBgra32f[base + 0U] = b;
        dstBgra32f[base + 1U] = g;
        dstBgra32f[base + 2U] = r;
        dstBgra32f[base + 3U] = 1.0f;
    }
    return true;
}

} // namespace openmedia::importer
