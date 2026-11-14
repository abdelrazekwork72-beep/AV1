#pragma once
/// @file pixel_format_8u.hpp
/// @brief 8-bit pixel format negotiation and WebM YUVA420P-to-BGRA8 packing engine for Module 2 (US-2.2.0 Unit 2).

#include "PrSDKPixelFormat.h"
#ifndef PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709
#define PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709 \
    PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u
#endif
#include "importer/importer_types.hpp"

#include <cstdint>

namespace openmedia::importer {

/// @brief Non-owning planar view of an 8-bit YUV420P or YUVA420P decoded video frame.
struct Yuva420pPlaneView {
    const uint8_t* yPlane = nullptr;   ///< Pointer to the 8-bit luma (Y) plane.
    const uint8_t* uPlane = nullptr;   ///< Pointer to the 8-bit chroma-blue (U/Cb) plane.
    const uint8_t* vPlane = nullptr;   ///< Pointer to the 8-bit chroma-red (V/Cr) plane.
    const uint8_t* aPlane = nullptr;   ///< Optional pointer to the 8-bit alpha plane (nullptr = opaque 255).
    int32_t yStride = 0;               ///< Byte stride of the Y plane.
    int32_t uStride = 0;               ///< Byte stride of the U plane.
    int32_t vStride = 0;               ///< Byte stride of the V plane.
    int32_t aStride = 0;               ///< Byte stride of the A plane.
    int32_t width = 0;                 ///< Frame width in pixels.
    int32_t height = 0;                ///< Frame height in pixels.
};

/// @brief Engine handling 8-bit pixel format negotiation (imGetIndPixelFormat) and YUVA420P-to-BGRA8 alpha packing.
class PixelFormat8uEngine {
public:
    /// @brief Returns the supported 8-bit Premiere pixel format at the requested enumeration index.
    /// @param index Zero-based pixel format index queried by MediaCore.
    /// @param hasAlpha True if the source stream carries an alpha channel (e.g., WebM VP8/VP9/AV1 YUVA420P).
    /// @param outFmt Output parameter receiving the negotiated PrPixelFormat FourCC constant.
    /// @return kImNoErr when a valid format exists at index, or kImBadFormatIndex when exhausted.
    static csSDK_int32 getSupported8uFormat(int32_t index,
                                            bool hasAlpha,
                                            csSDK_uint32& outFmt) noexcept;

    /// @brief Converts an 8-bit YUV420P or YUVA420P frame into packed BGRA_4444_8u while preserving full 8-bit alpha.
    /// @param src Planar source frame view containing Y, U, V, and optional A planes.
    /// @param dstBgra Destination BGRA8 buffer pointer (4 bytes per pixel: B, G, R, A).
    /// @param dstStride Destination row stride in bytes (must be >= src.width * 4).
    /// @return kImNoErr on success, or kImMemErr if pointers, dimensions, or strides are invalid.
    static csSDK_int32 packYuva420pToBgra8(const Yuva420pPlaneView& src,
                                           uint8_t* dstBgra,
                                           int32_t dstStride) noexcept;
};

} // namespace openmedia::importer
