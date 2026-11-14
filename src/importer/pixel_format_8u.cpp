#include "importer/pixel_format_8u.hpp"

#include <array>

namespace openmedia::importer {
namespace {

constexpr std::array<csSDK_uint32, 2> kAlpha8uFormats = {
    kPixFmtBGRA4444_8u,
    kPixFmtVUYA4444_8u,
};

constexpr std::array<csSDK_uint32, 3> kOpaque8uFormats = {
    kPixFmtYUV420Planar_8u_709,
    kPixFmtBGRA4444_8u,
    kPixFmtVUYA4444_8u,
};

uint8_t clampByte(int32_t value) noexcept {
    if (value < 0) {
        return 0U;
    }
    if (value > 255) {
        return 255U;
    }
    return static_cast<uint8_t>(value);
}

bool hasValidPointers(const Yuva420pPlaneView& src, const uint8_t* dstBgra) noexcept {
    if (src.yPlane == nullptr || src.uPlane == nullptr) {
        return false;
    }
    if (src.vPlane == nullptr || dstBgra == nullptr) {
        return false;
    }
    return true;
}

bool hasValidStrides(const Yuva420pPlaneView& src, int32_t dstStride) noexcept {
    if (src.width <= 0 || src.height <= 0) {
        return false;
    }
    const int32_t minChromaStride = (src.width + 1) / 2;
    if (src.yStride < src.width || dstStride < src.width * 4) {
        return false;
    }
    if (src.uStride < minChromaStride || src.vStride < minChromaStride) {
        return false;
    }
    if (src.aPlane != nullptr && src.aStride < src.width) {
        return false;
    }
    return true;
}

void yuv709ToBgraPixel(uint8_t y, uint8_t u, uint8_t v, uint8_t a, uint8_t* dst) noexcept {
    const int32_t c = static_cast<int32_t>(y) - 16;
    const int32_t d = static_cast<int32_t>(u) - 128;
    const int32_t e = static_cast<int32_t>(v) - 128;
    const int32_t r = (298 * c + 459 * e + 128) >> 8;
    const int32_t g = (298 * c - 55 * d - 136 * e + 128) >> 8;
    const int32_t b = (298 * c + 541 * d + 128) >> 8;
    dst[0] = clampByte(b);
    dst[1] = clampByte(g);
    dst[2] = clampByte(r);
    dst[3] = a;
}

void packRow(const Yuva420pPlaneView& src, int32_t row, uint8_t* dstRow) noexcept {
    const int32_t uvRow = row >> 1;
    const uint8_t* yRow = src.yPlane + (row * src.yStride);
    const uint8_t* uRow = src.uPlane + (uvRow * src.uStride);
    const uint8_t* vRow = src.vPlane + (uvRow * src.vStride);
    const uint8_t* aRow = (src.aPlane != nullptr) ? (src.aPlane + (row * src.aStride)) : nullptr;

    for (int32_t col = 0; col < src.width; ++col) {
        const int32_t uvCol = col >> 1;
        const uint8_t alpha = (aRow != nullptr) ? aRow[col] : 255U;
        yuv709ToBgraPixel(yRow[col], uRow[uvCol], vRow[uvCol], alpha, dstRow + (col * 4));
    }
}

} // namespace

csSDK_int32 PixelFormat8uEngine::getSupported8uFormat(int32_t index,
                                                       bool hasAlpha,
                                                       csSDK_uint32& outFmt) noexcept {
    if (index < 0) {
        return kImBadFormatIndex;
    }
    if (hasAlpha) {
        if (index >= static_cast<int32_t>(kAlpha8uFormats.size())) {
            return kImBadFormatIndex;
        }
        outFmt = kAlpha8uFormats[static_cast<size_t>(index)];
        return kImNoErr;
    }
    if (index >= static_cast<int32_t>(kOpaque8uFormats.size())) {
        return kImBadFormatIndex;
    }
    outFmt = kOpaque8uFormats[static_cast<size_t>(index)];
    return kImNoErr;
}

csSDK_int32 PixelFormat8uEngine::packYuva420pToBgra8(const Yuva420pPlaneView& src,
                                                      uint8_t* dstBgra,
                                                      int32_t dstStride) noexcept {
    if (!hasValidPointers(src, dstBgra) || !hasValidStrides(src, dstStride)) {
        return kImMemErr;
    }
    for (int32_t row = 0; row < src.height; ++row) {
        packRow(src, row, dstBgra + (row * dstStride));
    }
    return kImNoErr;
}

} // namespace openmedia::importer
