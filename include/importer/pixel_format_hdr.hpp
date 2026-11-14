#pragma once
/// @file pixel_format_hdr.hpp
/// @brief 10-bit and 12-bit HDR pixel format negotiation and 32-bit float linear conversion engine.

#include "importer/importer_types.hpp"

#include <cstddef>
#include <cstdint>

namespace openmedia::importer {

/// @brief Descriptor for an advertised high-bit-depth / HDR Premiere Pro pixel format.
struct HdrFormatDescriptor {
    /// @brief Adobe MediaCore pixel format FourCC / enum identifier.
    csSDK_uint32 pixelFormat = 0U;
    /// @brief Human-readable format name for diagnostics.
    const char* name = "";
    /// @brief Bits per color component (10 for 10u, 32 for 32f).
    std::uint8_t bitsPerComponent = 0U;
    /// @brief Number of color/alpha channels in the format.
    std::uint8_t channels = 0U;
    /// @brief True when the format carries linear 32-bit floating-point samples.
    bool isLinearFloat = false;
};

/// @brief Stream metadata used to negotiate 10-bit and 12-bit HDR pixel formats.
struct HdrStreamInfo {
    /// @brief Encoded stream bit depth (10 or 12 for HDR).
    std::uint8_t bitDepth = 10U;
    /// @brief True when the source stream uses 4:2:2 chroma subsampling.
    bool is422 = false;
    /// @brief True when the source stream carries an alpha plane.
    bool hasAlpha = false;
    /// @brief True when linear 32-bit float output is preferred.
    bool preferLinearFloat = true;
};

/// @brief Engine for 10/12-bit HDR pixel format negotiation and lossless float conversion.
class PixelFormatHdrEngine {
public:
    /// @brief Number of HDR pixel formats advertised in the static format table.
    static constexpr std::size_t kHdrFormatCount = 3U;

    /// @brief Checks whether a bit depth qualifies as 10-bit or 12-bit HDR.
    /// @param bitDepth Source video bit depth.
    /// @return True if bitDepth is 10 or 12, false otherwise.
    static bool isHdrBitDepth(std::uint8_t bitDepth) noexcept;

    /// @brief Retrieves a descriptor from the static HDR format table by index.
    /// @param index Zero-based format index in [0, kHdrFormatCount).
    /// @return Pointer to descriptor, or nullptr if index is out of range.
    static const HdrFormatDescriptor* getFormatDescriptor(std::size_t index) noexcept;

    /// @brief Checks whether a pixel format identifier is in the supported HDR table.
    /// @param pixelFormat MediaCore pixel format identifier.
    /// @return True if supported by PixelFormatHdrEngine.
    static bool isSupportedHdrFormat(csSDK_uint32 pixelFormat) noexcept;

    /// @brief Negotiates an HDR pixel format for imGetIndPixelFormat.
    /// @param rec Input/output pixel format enumeration record.
    /// @param streamInfo Metadata describing the source HDR stream.
    /// @return kImNoErr on success, kImUnsupported for non-HDR bit depths, or kImBadFormatIndex when index is out of bounds.
    static csSDK_int32 negotiateFormat(ImIndPixelFormatRecExt& rec,
                                       const HdrStreamInfo& streamInfo) noexcept;

    /// @brief Normalizes a 10-bit or 12-bit unsigned sample to [0.0f, 1.0f] without 8-bit truncation.
    /// @param sample Unsigned sample value in [0, 1023] (10-bit) or [0, 4095] (12-bit).
    /// @param bitDepth Source bit depth (10 or 12).
    /// @return Normalized 32-bit float in [0.0f, 1.0f], or 0.0f if bitDepth is unsupported.
    static float normalizeSampleToFloat(std::uint16_t sample,
                                        std::uint8_t bitDepth) noexcept;

    /// @brief Converts a single 10/12-bit YUV triplet to normalized 32-bit float linear RGB [0.0f, 1.0f].
    /// @param y Luma sample.
    /// @param u Chroma-blue (Cb) sample.
    /// @param v Chroma-red (Cr) sample.
    /// @param bitDepth Source bit depth (10 or 12).
    /// @param outR Output red channel in [0.0f, 1.0f].
    /// @param outG Output green channel in [0.0f, 1.0f].
    /// @param outB Output blue channel in [0.0f, 1.0f].
    /// @return True on success, false if bitDepth is not 10 or 12.
    static bool convertYuvSampleToRgb32f(std::uint16_t y,
                                         std::uint16_t u,
                                         std::uint16_t v,
                                         std::uint8_t bitDepth,
                                         float& outR,
                                         float& outG,
                                         float& outB) noexcept;

    /// @brief Converts 10/12-bit planar YUV samples into interleaved BGRA_4444_32f_Linear pixels.
    /// @param yPlane Pointer to luma samples (length >= pixelCount).
    /// @param uPlane Pointer to Cb samples (length >= pixelCount).
    /// @param vPlane Pointer to Cr samples (length >= pixelCount).
    /// @param pixelCount Number of pixels to convert.
    /// @param bitDepth Source bit depth (10 or 12).
    /// @param dstBgra32f Destination buffer for interleaved [B, G, R, A] 32-bit floats.
    /// @param dstFloatCount Capacity of dstBgra32f in float elements (must be >= pixelCount * 4).
    /// @return True on success, false on invalid pointers, insufficient buffer, or unsupported bit depth.
    static bool convertYuvPlanesToBgra32f(const std::uint16_t* yPlane,
                                          const std::uint16_t* uPlane,
                                          const std::uint16_t* vPlane,
                                          std::size_t pixelCount,
                                          std::uint8_t bitDepth,
                                          float* dstBgra32f,
                                          std::size_t dstFloatCount) noexcept;
};

} // namespace openmedia::importer
