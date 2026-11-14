#include "importer/stride_align.hpp"

#include <cstring>

namespace openmedia::importer {

namespace {

bool validateCopyBuffers(const uint8_t* srcPixels,
                         uint32_t srcStride,
                         uint8_t* dstPixels,
                         uint32_t width,
                         uint32_t height,
                         uint32_t bytesPerPixel) noexcept {
    if (srcPixels == nullptr || dstPixels == nullptr || height == 0) {
        return false;
    }
    const uint64_t rowCopyBytes =
        static_cast<uint64_t>(width) * static_cast<uint64_t>(bytesPerPixel);
    return static_cast<uint64_t>(srcStride) >= rowCopyBytes;
}

} // namespace

uint32_t StrideAlignmentEngine::computeAlignedStride16(
    uint32_t width,
    uint32_t bytesPerPixel) noexcept {
    if (width == 0 || bytesPerPixel == 0) {
        return 0;
    }
    const uint64_t rawBytes =
        static_cast<uint64_t>(width) * static_cast<uint64_t>(bytesPerPixel);
    const uint64_t aligned = ((rawBytes + 15ULL) / 16ULL) * 16ULL;
    if (aligned > static_cast<uint64_t>(UINT32_MAX)) {
        return 0;
    }
    return static_cast<uint32_t>(aligned);
}

bool StrideAlignmentEngine::isValidAlignedStride(
    int32_t rowBytes,
    uint32_t width,
    uint32_t bytesPerPixel) noexcept {
    if (rowBytes <= 0 || width == 0 || bytesPerPixel == 0) {
        return false;
    }
    if ((rowBytes % 16) != 0) {
        return false;
    }
    const uint64_t minBytes =
        static_cast<uint64_t>(width) * static_cast<uint64_t>(bytesPerPixel);
    return static_cast<uint64_t>(rowBytes) >= minBytes;
}

bool StrideAlignmentEngine::copyWithRowBytes(
    const uint8_t* srcPixels,
    uint32_t srcStride,
    uint8_t* dstPixels,
    int32_t rowBytes,
    uint32_t width,
    uint32_t height,
    uint32_t bytesPerPixel) noexcept {
    if (!isValidAlignedStride(rowBytes, width, bytesPerPixel)) {
        return false;
    }
    if (!validateCopyBuffers(srcPixels, srcStride, dstPixels, width, height, bytesPerPixel)) {
        return false;
    }
    const size_t rowCopyBytes =
        static_cast<size_t>(width) * static_cast<size_t>(bytesPerPixel);
    const size_t dstStride = static_cast<size_t>(rowBytes);
    const size_t padBytes = dstStride - rowCopyBytes;

    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* srcRow = srcPixels + static_cast<size_t>(y) * srcStride;
        uint8_t* dstRow = dstPixels + static_cast<size_t>(y) * dstStride;
        std::memcpy(dstRow, srcRow, rowCopyBytes);
        std::memset(dstRow + rowCopyBytes, 0, padBytes);
    }
    return true;
}

} // namespace openmedia::importer
