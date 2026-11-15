#pragma once
/// @file pixel_format_hdr.hpp
/// @brief 10/12-bit HDR negotiation, GPU/SIMD NV12/P010 direct conversion, and extended camera color spaces.

#include "importer/importer_types.hpp"

#include <cstddef>
#include <cstdint>

namespace openmedia::importer {

/// @brief Extended color transfer and gamut spaces supported by Influx-v2 / OpenMedia.
enum class ExtendedColorSpace : std::uint8_t {
    sRGB = 0,
    BT601,
    BT709,
    BT2020,
    BT2100_HLG,
    BT2100_PQ,
    DCDM_XYZ,
    Sony_SGamut_SLog2,
    Sony_SGamut3_SLog3,
    Sony_Cine_SLog3
};

/// @brief Descriptor for an extended camera / broadcast color space.
struct ColorTransferParams {
    ExtendedColorSpace space = ExtendedColorSpace::BT709;
    const char* name = "BT.709";
    bool isHdr = false;
    bool isWideGamut = false;
    float referenceGamma = 2.4f;
};

/// @brief Descriptor for an advertised high-bit-depth / HDR Premiere Pro pixel format.
struct HdrFormatDescriptor {
    csSDK_uint32 pixelFormat = 0U;
    const char* name = "";
    std::uint8_t bitsPerComponent = 0U;
    std::uint8_t channels = 0U;
    bool isLinearFloat = false;
};

/// @brief Stream metadata used to negotiate 10-bit and 12-bit HDR pixel formats.
struct HdrStreamInfo {
    std::uint8_t bitDepth = 10U;
    bool is422 = false;
    bool hasAlpha = false;
    bool preferLinearFloat = true;
};

/// @brief Engine for 10/12-bit HDR negotiation, NV12/P010 direct conversion, and camera color spaces.
class PixelFormatHdrEngine {
public:
    static constexpr std::size_t kHdrFormatCount = 3U;

    static bool isHdrBitDepth(std::uint8_t bitDepth) noexcept;
    static const HdrFormatDescriptor* getFormatDescriptor(std::size_t index) noexcept;
    static bool isSupportedHdrFormat(csSDK_uint32 pixelFormat) noexcept;
    static csSDK_int32 negotiateFormat(ImIndPixelFormatRecExt& rec,
                                       const HdrStreamInfo& streamInfo) noexcept;
    static float normalizeSampleToFloat(std::uint16_t sample,
                                        std::uint8_t bitDepth) noexcept;
    static bool convertYuvSampleToRgb32f(std::uint16_t y,
                                         std::uint16_t u,
                                         std::uint16_t v,
                                         std::uint8_t bitDepth,
                                         float& outR,
                                         float& outG,
                                         float& outB) noexcept;
    static bool convertYuvPlanesToBgra32f(const std::uint16_t* yPlane,
                                          const std::uint16_t* uPlane,
                                          const std::uint16_t* vPlane,
                                          std::size_t pixelCount,
                                          std::uint8_t bitDepth,
                                          float* dstBgra32f,
                                          std::size_t dstFloatCount) noexcept;

    /// @brief Returns metadata for an extended camera / broadcast color space.
    static ColorTransferParams getColorSpaceMapping(ExtendedColorSpace space) noexcept;

    /// @brief Linearizes a normalized [0.0, 1.0] signal using the target transfer curve.
    static float applyTransferToLinear(float encoded, ExtendedColorSpace space) noexcept;

    /// @brief Direct NV12 -> BGRA8 fast-path (cuda_bgra_nv12 equivalent).
    static bool convertNv12ToBgra8Direct(const std::uint8_t* yPlane,
                                         const std::uint8_t* uvPlane,
                                         std::uint32_t yStride,
                                         std::uint32_t uvStride,
                                         std::uint32_t width,
                                         std::uint32_t height,
                                         std::uint8_t* dstBgra,
                                         std::uint32_t dstStride,
                                         bool fullRange = false) noexcept;

    /// @brief Direct P010LE -> BGRA8 or RGB444_10U fast-path (cuda_bgra_p010 equivalent).
    static bool convertP010ToBgra8OrRgb444Direct(const std::uint16_t* yPlane,
                                                  const std::uint16_t* uvPlane,
                                                  std::uint32_t yStrideSamples,
                                                  std::uint32_t uvStrideSamples,
                                                  std::uint32_t width,
                                                  std::uint32_t height,
                                                  std::uint8_t* dstBuffer,
                                                  std::uint32_t dstStrideBytes,
                                                  bool toRgb444_10u = false) noexcept;
};

} // namespace openmedia::importer
