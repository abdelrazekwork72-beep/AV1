/// @file frame_scaler.cpp
/// @brief Implementation of FrameScaler with cached SwsContext and Rec.601/709/2020 matrices.

#include "importer/frame_scaler.hpp"

#include <algorithm>
#include <cstring>

#ifdef OPENMEDIA_HAS_FFMPEG
extern "C" {
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}
#endif

namespace openmedia::importer {

namespace {

constexpr int kSwsCsItu709 = 1;
constexpr int kSwsCsItu601 = 5;
constexpr int kSwsCsBt2020 = 9;

bool isValidConfig(const ScaleConfig& cfg) noexcept {
    const bool validDims = cfg.srcWidth > 0 && cfg.srcHeight > 0 &&
                           cfg.dstWidth > 0 && cfg.dstHeight > 0;
    const bool validFmts = cfg.srcFormat != openmedia::PixelFormat::Unknown &&
                           cfg.dstFormat != openmedia::PixelFormat::Unknown;
    return validDims && validFmts;
}

bool hasValidPlanes(const uint8_t* const planes[4],
                    const int32_t strides[4]) noexcept {
    if (planes == nullptr || strides == nullptr) {
        return false;
    }
    return planes[0] != nullptr && strides[0] > 0;
}

#ifndef OPENMEDIA_HAS_FFMPEG
uint8_t clampByte(int32_t value) noexcept {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return static_cast<uint8_t>(value);
}
#endif

#ifdef OPENMEDIA_HAS_FFMPEG

AVPixelFormat toAvPixelFormat(openmedia::PixelFormat fmt) noexcept {
    struct AvFmtEntry {
        openmedia::PixelFormat src;
        AVPixelFormat dst;
    };
    static constexpr AvFmtEntry kTable[] = {
        {openmedia::PixelFormat::YUV420P, AV_PIX_FMT_YUV420P},
        {openmedia::PixelFormat::YUV422P, AV_PIX_FMT_YUV422P},
        {openmedia::PixelFormat::YUV444P, AV_PIX_FMT_YUV444P},
        {openmedia::PixelFormat::YUV420P10LE, AV_PIX_FMT_YUV420P10LE},
        {openmedia::PixelFormat::YUV422P10LE, AV_PIX_FMT_YUV422P10LE},
        {openmedia::PixelFormat::YUV444P10LE, AV_PIX_FMT_YUV444P10LE},
        {openmedia::PixelFormat::YUV420P12LE, AV_PIX_FMT_YUV420P12LE},
        {openmedia::PixelFormat::YUV422P12LE, AV_PIX_FMT_YUV422P12LE},
        {openmedia::PixelFormat::YUV444P12LE, AV_PIX_FMT_YUV444P12LE},
        {openmedia::PixelFormat::BGRA8, AV_PIX_FMT_BGRA},
        {openmedia::PixelFormat::RGBA8, AV_PIX_FMT_RGBA},
        {openmedia::PixelFormat::NV12, AV_PIX_FMT_NV12},
        {openmedia::PixelFormat::P010LE, AV_PIX_FMT_P010LE},
    };
    for (const auto& entry : kTable) {
        if (entry.src == fmt) {
            return entry.dst;
        }
    }
    return AV_PIX_FMT_NONE;
}

void applySwsColorspace(SwsContext* ctx,
                        ColorMatrix matrix,
                        openmedia::ColorRange range) noexcept {
    const int* table = sws_getCoefficients(FrameScaler::toSwsColorspaceId(matrix));
    const int srcRange = (range == openmedia::ColorRange::Full) ? 1 : 0;
    sws_setColorspaceDetails(ctx, table, srcRange, table, 1, 0, 1 << 16, 1 << 16);
}

#else

struct YuvRgbCoeffs {
    int32_t crToR;
    int32_t cbToG;
    int32_t crToG;
    int32_t cbToB;
};

YuvRgbCoeffs getFallbackCoeffs(ColorMatrix matrix) noexcept {
    if (matrix == ColorMatrix::Rec601) return {1436, 352, 731, 1815};
    if (matrix == ColorMatrix::Rec2020) return {1510, 169, 585, 1927};
    return {1612, 192, 479, 1900};
}

void writeFallbackPixel(uint8_t* dstPixel,
                        openmedia::PixelFormat dstFormat,
                        uint8_t r,
                        uint8_t g,
                        uint8_t b) noexcept {
    if (dstFormat == openmedia::PixelFormat::RGBA8) {
        dstPixel[0] = r;
        dstPixel[1] = g;
        dstPixel[2] = b;
        dstPixel[3] = 255;
        return;
    }
    dstPixel[0] = b;
    dstPixel[1] = g;
    dstPixel[2] = r;
    dstPixel[3] = 255;
}

void convertYuv420Row(const uint8_t* yRow,
                      const uint8_t* uRow,
                      const uint8_t* vRow,
                      uint8_t* dstRow,
                      const ScaleConfig& config,
                      const YuvRgbCoeffs& coeffs) noexcept {
    const int32_t yOffset = (config.colorRange == openmedia::ColorRange::Full) ? 0 : 16;
    for (int32_t x = 0; x < config.dstWidth; ++x) {
        const int32_t srcX = (x * config.srcWidth) / config.dstWidth;
        const int32_t yVal = static_cast<int32_t>(yRow[srcX]) - yOffset;
        const int32_t uVal = static_cast<int32_t>(uRow[srcX / 2]) - 128;
        const int32_t vVal = static_cast<int32_t>(vRow[srcX / 2]) - 128;

        const int32_t r = yVal + ((coeffs.crToR * vVal) >> 10);
        const int32_t g = yVal - ((coeffs.cbToG * uVal + coeffs.crToG * vVal) >> 10);
        const int32_t b = yVal + ((coeffs.cbToB * uVal) >> 10);
        writeFallbackPixel(dstRow + static_cast<size_t>(x) * 4U,
                           config.dstFormat,
                           clampByte(r),
                           clampByte(g),
                           clampByte(b));
    }
}

void fallbackScaleYuv420(const ScaleConfig& config,
                        const uint8_t* const srcPlanes[4],
                        const int32_t srcStrides[4],
                        uint8_t* const dstPlanes[4],
                        const int32_t dstStrides[4]) noexcept {
    const YuvRgbCoeffs coeffs = getFallbackCoeffs(config.colorMatrix);
    const bool hasChroma = srcPlanes[1] != nullptr && srcPlanes[2] != nullptr &&
                           srcStrides[1] > 0 && srcStrides[2] > 0;
    for (int32_t y = 0; y < config.dstHeight; ++y) {
        const int32_t srcY = (y * config.srcHeight) / config.dstHeight;
        const uint8_t* yRow = srcPlanes[0] + static_cast<size_t>(srcY) * srcStrides[0];
        const uint8_t* uRow = hasChroma ? srcPlanes[1] + static_cast<size_t>(srcY / 2) * srcStrides[1] : yRow;
        const uint8_t* vRow = hasChroma ? srcPlanes[2] + static_cast<size_t>(srcY / 2) * srcStrides[2] : yRow;
        uint8_t* dstRow = dstPlanes[0] + static_cast<size_t>(y) * dstStrides[0];
        convertYuv420Row(yRow, uRow, vRow, dstRow, config, coeffs);
    }
}

void fallbackCopyRows(const ScaleConfig& config,
                      const uint8_t* const srcPlanes[4],
                      const int32_t srcStrides[4],
                      uint8_t* const dstPlanes[4],
                      const int32_t dstStrides[4]) noexcept {
    const int32_t copyWidth = std::min(srcStrides[0], dstStrides[0]);
    const int32_t rows = std::min(config.srcHeight, config.dstHeight);
    for (int32_t y = 0; y < rows; ++y) {
        std::memcpy(dstPlanes[0] + static_cast<size_t>(y) * dstStrides[0],
                    srcPlanes[0] + static_cast<size_t>(y) * srcStrides[0],
                    static_cast<size_t>(copyWidth));
    }
}

#endif // OPENMEDIA_HAS_FFMPEG

} // namespace

struct FrameScaler::Impl {
#ifdef OPENMEDIA_HAS_FFMPEG
    SwsContext* context = nullptr;
#else
    bool hasContext = false;
#endif
    ScaleConfig cachedConfig{};
    uint64_t cacheMisses = 0;
    uint64_t scales = 0;
    ColorMatrix lastMatrix = ColorMatrix::Rec709;

    ~Impl() {
#ifdef OPENMEDIA_HAS_FFMPEG
        if (context != nullptr) {
            sws_freeContext(context);
            context = nullptr;
        }
#endif
    }

    bool ensureContext(const ScaleConfig& config) noexcept {
#ifdef OPENMEDIA_HAS_FFMPEG
        const AVPixelFormat srcAvFmt = toAvPixelFormat(config.srcFormat);
        const AVPixelFormat dstAvFmt = toAvPixelFormat(config.dstFormat);
        if (srcAvFmt == AV_PIX_FMT_NONE || dstAvFmt == AV_PIX_FMT_NONE) {
            return false;
        }

        const bool configChanged = (context == nullptr) || (cachedConfig != config);
        SwsContext* nextCtx = sws_getCachedContext(
            context,
            config.srcWidth,
            config.srcHeight,
            srcAvFmt,
            config.dstWidth,
            config.dstHeight,
            dstAvFmt,
            SWS_BICUBIC,
            nullptr,
            nullptr,
            nullptr);
        if (nextCtx == nullptr) {
            return false;
        }

        if (configChanged) {
            ++cacheMisses;
            cachedConfig = config;
            applySwsColorspace(nextCtx, config.colorMatrix, config.colorRange);
        }
        context = nextCtx;
        lastMatrix = config.colorMatrix;
        return true;
#else
        if (!hasContext || cachedConfig != config) {
            hasContext = true;
            cachedConfig = config;
            ++cacheMisses;
        }
        lastMatrix = config.colorMatrix;
        return true;
#endif
    }

    void executeScale(const ScaleConfig& config,
                      const uint8_t* const srcPlanes[4],
                      const int32_t srcStrides[4],
                      uint8_t* const dstPlanes[4],
                      const int32_t dstStrides[4]) noexcept {
#ifdef OPENMEDIA_HAS_FFMPEG
        sws_scale(context,
                  srcPlanes,
                  srcStrides,
                  0,
                  config.srcHeight,
                  dstPlanes,
                  dstStrides);
#else
        if (config.srcFormat == openmedia::PixelFormat::YUV420P &&
            (config.dstFormat == openmedia::PixelFormat::BGRA8 ||
             config.dstFormat == openmedia::PixelFormat::RGBA8)) {
            fallbackScaleYuv420(config, srcPlanes, srcStrides, dstPlanes, dstStrides);
            return;
        }
        fallbackCopyRows(config, srcPlanes, srcStrides, dstPlanes, dstStrides);
#endif
    }
};

FrameScaler::FrameScaler() : impl_(std::make_unique<Impl>()) {}

FrameScaler::~FrameScaler() = default;

FrameScaler::FrameScaler(FrameScaler&& other) noexcept = default;

FrameScaler& FrameScaler::operator=(FrameScaler&& other) noexcept = default;

openmedia::CoreStatus FrameScaler::scale(const ScaleConfig& config,
                                         const uint8_t* const srcPlanes[4],
                                         const int32_t srcStrides[4],
                                         uint8_t* const dstPlanes[4],
                                         const int32_t dstStrides[4]) noexcept {
    if (!impl_ || !isValidConfig(config)) {
        return openmedia::CoreStatus::InvalidArgument;
    }
    if (!hasValidPlanes(srcPlanes, srcStrides) || !hasValidPlanes(dstPlanes, dstStrides)) {
        return openmedia::CoreStatus::InvalidArgument;
    }
    if (!impl_->ensureContext(config)) {
        return openmedia::CoreStatus::DecodeFailed;
    }

    impl_->executeScale(config, srcPlanes, srcStrides, dstPlanes, dstStrides);
    ++impl_->scales;
    return openmedia::CoreStatus::Ok;
}

openmedia::CoreStatus FrameScaler::scaleFrame(const openmedia::RawVideoFrame& srcFrame,
                                              uint8_t* dstBuffer,
                                              int32_t dstStride,
                                              openmedia::PixelFormat dstFormat,
                                              int32_t dstWidth,
                                              int32_t dstHeight) noexcept {
    if (dstBuffer == nullptr || dstStride <= 0 || srcFrame.planes[0].empty()) {
        return openmedia::CoreStatus::InvalidArgument;
    }

    ScaleConfig config;
    config.srcWidth = srcFrame.width;
    config.srcHeight = srcFrame.height;
    config.srcFormat = srcFrame.format;
    config.dstWidth = (dstWidth > 0) ? dstWidth : srcFrame.width;
    config.dstHeight = (dstHeight > 0) ? dstHeight : srcFrame.height;
    config.dstFormat = dstFormat;
    config.colorMatrix = mapColorSpace(srcFrame.colorSpace);
    config.colorRange = srcFrame.colorRange;

    const uint8_t* srcPlanes[4] = {
        srcFrame.planes[0].data(),
        srcFrame.planes[1].empty() ? nullptr : srcFrame.planes[1].data(),
        srcFrame.planes[2].empty() ? nullptr : srcFrame.planes[2].data(),
        srcFrame.planes[3].empty() ? nullptr : srcFrame.planes[3].data()
    };
    const int32_t srcStrides[4] = {
        static_cast<int32_t>(srcFrame.rowBytes[0]),
        static_cast<int32_t>(srcFrame.rowBytes[1]),
        static_cast<int32_t>(srcFrame.rowBytes[2]),
        static_cast<int32_t>(srcFrame.rowBytes[3])
    };
    uint8_t* dstPlanes[4] = {dstBuffer, nullptr, nullptr, nullptr};
    const int32_t dstStrides[4] = {dstStride, 0, 0, 0};

    return scale(config, srcPlanes, srcStrides, dstPlanes, dstStrides);
}

void FrameScaler::reset() noexcept {
    if (!impl_) {
        return;
    }
#ifdef OPENMEDIA_HAS_FFMPEG
    if (impl_->context != nullptr) {
        sws_freeContext(impl_->context);
        impl_->context = nullptr;
    }
#else
    impl_->hasContext = false;
#endif
    impl_->cachedConfig = ScaleConfig{};
    impl_->cacheMisses = 0;
    impl_->scales = 0;
    impl_->lastMatrix = ColorMatrix::Rec709;
}

uint64_t FrameScaler::cacheMissCount() const noexcept {
    return impl_ ? impl_->cacheMisses : 0;
}

uint64_t FrameScaler::scaleCount() const noexcept {
    return impl_ ? impl_->scales : 0;
}

ColorMatrix FrameScaler::activeColorMatrix() const noexcept {
    return impl_ ? impl_->lastMatrix : ColorMatrix::Rec709;
}

ColorMatrix FrameScaler::mapColorSpace(openmedia::ColorSpace space) noexcept {
    if (space == openmedia::ColorSpace::BT601) return ColorMatrix::Rec601;
    if (space == openmedia::ColorSpace::BT2020_NCL || space == openmedia::ColorSpace::BT2020_CL) {
        return ColorMatrix::Rec2020;
    }
    return ColorMatrix::Rec709;
}

int FrameScaler::toSwsColorspaceId(ColorMatrix matrix) noexcept {
    if (matrix == ColorMatrix::Rec601) return kSwsCsItu601;
    if (matrix == ColorMatrix::Rec2020) return kSwsCsBt2020;
    return kSwsCsItu709;
}

std::array<int, 4> FrameScaler::getMatrixCoefficients(ColorMatrix matrix) noexcept {
    if (matrix == ColorMatrix::Rec601) return {104597, 132201, 25675, 53279};
    if (matrix == ColorMatrix::Rec2020) return {117504, 148078, 13954, 46750};
    return {117489, 138438, 13975, 34925};
}

} // namespace openmedia::importer
