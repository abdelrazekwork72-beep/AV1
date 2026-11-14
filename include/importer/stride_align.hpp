#pragma once
/// @file stride_align.hpp
/// @brief 16-byte GetRowBytes stride alignment engine for Adobe MediaCore video frame delivery.

#include <cstdint>

namespace openmedia::importer {

/// @brief Enforces 16-byte GetRowBytes stride alignment and safe row-by-row pixel copying.
class StrideAlignmentEngine {
public:
    /// @brief Computes the 16-byte aligned row stride for a given width and pixel byte size.
    /// @param width Frame width in pixels.
    /// @param bytesPerPixel Number of bytes per pixel (e.g., 4 for BGRA_4444_8u, 8 for BGRA_4444_16u).
    /// @return 16-byte aligned stride in bytes, or 0 if inputs are zero or overflow uint32_t.
    static uint32_t computeAlignedStride16(uint32_t width, uint32_t bytesPerPixel) noexcept;

    /// @brief Validates that a host-provided rowBytes value is 16-byte aligned and sufficient for the row.
    /// @param rowBytes Host-provided destination row stride in bytes (from GetRowBytes).
    /// @param width Frame width in pixels.
    /// @param bytesPerPixel Number of bytes per pixel.
    /// @return True if rowBytes > 0, rowBytes % 16 == 0, and rowBytes >= width * bytesPerPixel.
    static bool isValidAlignedStride(int32_t rowBytes,
                                     uint32_t width,
                                     uint32_t bytesPerPixel) noexcept;

    /// @brief Copies pixel rows from a source buffer into a 16-byte aligned destination buffer.
    /// @param srcPixels Pointer to the source pixel buffer.
    /// @param srcStride Source row stride in bytes (must be >= width * bytesPerPixel).
    /// @param dstPixels Pointer to the destination pixel buffer.
    /// @param rowBytes Host-provided destination row stride in bytes (dstStride = rowBytes).
    /// @param width Frame width in pixels.
    /// @param height Frame height in pixels.
    /// @param bytesPerPixel Number of bytes per pixel.
    /// @return True if validation succeeded and all rows were copied without overflow.
    static bool copyWithRowBytes(const uint8_t* srcPixels,
                                 uint32_t srcStride,
                                 uint8_t* dstPixels,
                                 int32_t rowBytes,
                                 uint32_t width,
                                 uint32_t height,
                                 uint32_t bytesPerPixel) noexcept;
};

} // namespace openmedia::importer
