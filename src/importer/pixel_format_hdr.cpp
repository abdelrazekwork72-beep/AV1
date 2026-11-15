/// @file pixel_format_hdr.cpp
/// @brief Implementation of PixelFormatHdrEngine with NV12/P010 direct paths and camera color spaces.

#include "importer/pixel_format_hdr.hpp"

#include <array>
#include <cmath>
#include <cstring>

namespace openmedia::importer {
namespace {

constexpr std::array<HdrFormatDescriptor, PixelFormatHdrEngine::kHdrFormatCount> kHdrFormatTable{{
    {kPixFmtRGB444_10u, "PrPixelFormat_RGB_444_10u", 10U, 3U, false},
    {kPixFmtUYVY422_32f, "PrPixelFormat_UYVY_422_32f", 32U, 3U, true},
    {kPixFmtBGRA4444_32f_Linear, "PrPixelFormat_BGRA_4444_32f_Linear", 32U, 4U, true},
}};

constexpr std::array<ColorTransferParams, 10> kColorSpaceTable{{
    {ExtendedColorSpace::sRGB, "sRGB", false, false, 2.2f},
    {ExtendedColorSpace::BT601, "BT.601", false, false, 2.4f},
    {ExtendedColorSpace::BT709, "BT.709", false, false, 2.4f},
    {ExtendedColorSpace::BT2020, "BT.2020", false, true, 2.4f},
    {ExtendedColorSpace::BT2100_HLG, "BT.2100_HLG", true, true, 1.2f},
    {ExtendedColorSpace::BT2100_PQ, "BT.2100_PQ", true, true, 2.4f},
    {ExtendedColorSpace::DCDM_XYZ, "DCDM_XYZ", true, true, 2.6f},
    {ExtendedColorSpace::Sony_SGamut_SLog2, "Sony_SGamut_SLog2", true, true, 2.4f},
    {ExtendedColorSpace::Sony_SGamut3_SLog3, "Sony_SGamut3_SLog3", true, true, 2.4f},
    {ExtendedColorSpace::Sony_Cine_SLog3, "Sony_Cine_SLog3", true, true, 2.4f},
}};

float clampUnitFloat(float value) noexcept {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

std::uint8_t clampByteInt(int32_t value) noexcept {
    if (value < 0) return 0U;
    if (value > 255) return 255U;
    return static_cast<std::uint8_t>(value);
}

std::uint16_t clamp10Bit(float value) noexcept {
    const float clamped = clampUnitFloat(value);
    return static_cast<std::uint16_t>(clamped * 1023.0f + 0.5f);
}

bool arePlaneBuffersValid(const std::uint16_t* yPlane,
                          const std::uint16_t* uPlane,
                          const std::uint16_t* vPlane,
                          const float* dstBgra32f) noexcept {
    return (yPlane != nullptr) && (uPlane != nullptr) && (vPlane != nullptr) && (dstBgra32f != nullptr);
}

float linearizeSrgb(float v) noexcept {
    return (v <= 0.04045f) ? (v / 12.92f) : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

float linearizePq(float v) noexcept {
    constexpr float kM1 = 0.1593017578125f;
    constexpr float kM2 = 78.84375f;
    constexpr float kC1 = 0.8359375f;
    constexpr float kC2 = 18.8515625f;
    constexpr float kC3 = 18.6875f;
    const float vp = std::pow(clampUnitFloat(v), 1.0f / kM2);
    const float num = std::max(vp - kC1, 0.0f);
    const float den = kC2 - kC3 * vp;
    return (den > 1e-6f) ? std::pow(num / den, 1.0f / kM1) : 0.0f;
}

float linearizeHlg(float v) noexcept {
    constexpr float kA = 0.17883277f;
    constexpr float kB = 0.28466892f;
    constexpr float kC = 0.55991073f;
    return (v <= 0.5f) ? (v * v / 3.0f) : ((std::exp((v - kC) / kA) + kB) / 12.0f);
}

float linearizeSLog3(float v) noexcept {
    if (v >= 171.2102946929f / 1023.0f) {
        return std::pow(10.0f, (v * 1023.0f - 420.0f) / 261.5f) * (0.18f + 0.01f) - 0.01f;
    }
    return (v * 1023.0f - 95.0f) * 0.01125f / (171.2102946929f - 95.0f);
}

float linearizeSLog2(float v) noexcept {
    if (v < 90.0f / 1023.0f) {
        return (v * 1023.0f - 90.0f) * 0.0005f;
    }
    return 0.219f * std::pow(10.0f, (v - 0.616596f - 0.03f) / 0.432699f);
}

void writeP010Pixel(std::uint8_t* dstRow,
                    std::uint32_t x,
                    float r,
                    float g,
                    float b,
                    bool toRgb444_10u) noexcept {
    if (toRgb444_10u) {
        const std::uint32_t packed =
            (static_cast<std::uint32_t>(clamp10Bit(r)) << 20U) |
            (static_cast<std::uint32_t>(clamp10Bit(g)) << 10U) |
            static_cast<std::uint32_t>(clamp10Bit(b));
        std::memcpy(dstRow + static_cast<std::size_t>(x) * 4U, &packed, sizeof(packed));
        return;
    }
    std::uint8_t* px = dstRow + static_cast<std::size_t>(x) * 4U;
    px[0] = clampByteInt(static_cast<int32_t>(b * 255.0f + 0.5f));
    px[1] = clampByteInt(static_cast<int32_t>(g * 255.0f + 0.5f));
    px[2] = clampByteInt(static_cast<int32_t>(r * 255.0f + 0.5f));
    px[3] = 255U;
}

} // namespace

bool PixelFormatHdrEngine::isHdrBitDepth(std::uint8_t bitDepth) noexcept {
    return (bitDepth == 10U) || (bitDepth == 12U);
}

const HdrFormatDescriptor* PixelFormatHdrEngine::getFormatDescriptor(std::size_t index) noexcept {
    return (index < kHdrFormatCount) ? &kHdrFormatTable[index] : nullptr;
}

bool PixelFormatHdrEngine::isSupportedHdrFormat(csSDK_uint32 pixelFormat) noexcept {
    for (const auto& entry : kHdrFormatTable) {
        if (entry.pixelFormat == pixelFormat) {
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
        outR = outG = outB = 0.0f;
        return false;
    }
    const float yf = normalizeSampleToFloat(y, bitDepth);
    const float uf = normalizeSampleToFloat(u, bitDepth) - 0.5f;
    const float vf = normalizeSampleToFloat(v, bitDepth) - 0.5f;

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
        float r = 0.0f, g = 0.0f, b = 0.0f;
        convertYuvSampleToRgb32f(yPlane[i], uPlane[i], vPlane[i], bitDepth, r, g, b);
        const std::size_t base = i * 4U;
        dstBgra32f[base + 0U] = b;
        dstBgra32f[base + 1U] = g;
        dstBgra32f[base + 2U] = r;
        dstBgra32f[base + 3U] = 1.0f;
    }
    return true;
}

ColorTransferParams PixelFormatHdrEngine::getColorSpaceMapping(ExtendedColorSpace space) noexcept {
    const auto idx = static_cast<std::size_t>(space);
    return (idx < kColorSpaceTable.size()) ? kColorSpaceTable[idx] : kColorSpaceTable[2];
}

float linearizeGamma24(float v) noexcept { return std::pow(v, 2.4f); }
float linearizeGamma26(float v) noexcept { return std::pow(v, 2.6f); }

float PixelFormatHdrEngine::applyTransferToLinear(float encoded, ExtendedColorSpace space) noexcept {
    using TransferFn = float (*)(float) noexcept;
    static constexpr std::array<TransferFn, 10> kTransferFns = {
        &linearizeSrgb,
        &linearizeGamma24,
        &linearizeGamma24,
        &linearizeGamma24,
        &linearizeHlg,
        &linearizePq,
        &linearizeGamma26,
        &linearizeSLog2,
        &linearizeSLog3,
        &linearizeSLog3,
    };
    const float v = clampUnitFloat(encoded);
    const auto idx = static_cast<std::size_t>(space);
    return (idx < kTransferFns.size()) ? kTransferFns[idx](v) : linearizeGamma24(v);
}

bool PixelFormatHdrEngine::convertNv12ToBgra8Direct(const std::uint8_t* yPlane,
                                                    const std::uint8_t* uvPlane,
                                                    std::uint32_t yStride,
                                                    std::uint32_t uvStride,
                                                    std::uint32_t width,
                                                    std::uint32_t height,
                                                    std::uint8_t* dstBgra,
                                                    std::uint32_t dstStride,
                                                    bool fullRange) noexcept {
    if (!yPlane || !uvPlane || !dstBgra || width == 0U || height == 0U) {
        return false;
    }
    const int32_t yOff = fullRange ? 0 : 16;
    for (std::uint32_t y = 0U; y < height; ++y) {
        const std::uint8_t* yRow = yPlane + static_cast<std::size_t>(y) * yStride;
        const std::uint8_t* uvRow = uvPlane + static_cast<std::size_t>(y >> 1U) * uvStride;
        std::uint8_t* dstRow = dstBgra + static_cast<std::size_t>(y) * dstStride;
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::uint32_t uvIdx = (x & ~1U);
            const int32_t yv = static_cast<int32_t>(yRow[x]) - yOff;
            const int32_t uv = static_cast<int32_t>(uvRow[uvIdx]) - 128;
            const int32_t vv = static_cast<int32_t>(uvRow[uvIdx + 1U]) - 128;
            std::uint8_t* px = dstRow + static_cast<std::size_t>(x) * 4U;
            px[0] = clampByteInt(yv + ((1900 * uv) >> 10));
            px[1] = clampByteInt(yv - ((192 * uv + 479 * vv) >> 10));
            px[2] = clampByteInt(yv + ((1612 * vv) >> 10));
            px[3] = 255U;
        }
    }
    return true;
}

bool PixelFormatHdrEngine::convertP010ToBgra8OrRgb444Direct(const std::uint16_t* yPlane,
                                                             const std::uint16_t* uvPlane,
                                                             std::uint32_t yStrideSamples,
                                                             std::uint32_t uvStrideSamples,
                                                             std::uint32_t width,
                                                             std::uint32_t height,
                                                             std::uint8_t* dstBuffer,
                                                             std::uint32_t dstStrideBytes,
                                                             bool toRgb444_10u) noexcept {
    if (!yPlane || !uvPlane || !dstBuffer || width == 0U || height == 0U) {
        return false;
    }
    for (std::uint32_t y = 0U; y < height; ++y) {
        const std::uint16_t* yRow = yPlane + static_cast<std::size_t>(y) * yStrideSamples;
        const std::uint16_t* uvRow = uvPlane + static_cast<std::size_t>(y >> 1U) * uvStrideSamples;
        std::uint8_t* dstRow = dstBuffer + static_cast<std::size_t>(y) * dstStrideBytes;
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::uint32_t uvIdx = (x & ~1U);
            float r = 0.0f, g = 0.0f, b = 0.0f;
            convertYuvSampleToRgb32f(static_cast<std::uint16_t>(yRow[x] >> 6U),
                                     static_cast<std::uint16_t>(uvRow[uvIdx] >> 6U),
                                     static_cast<std::uint16_t>(uvRow[uvIdx + 1U] >> 6U),
                                     10U, r, g, b);
            writeP010Pixel(dstRow, x, r, g, b, toRgb444_10u);
        }
    }
    return true;
}

} // namespace openmedia::importer
