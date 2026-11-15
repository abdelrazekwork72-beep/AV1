#pragma once
/// @file frame_scaler.hpp
/// @brief Reusable SwsContext video frame scaler with extended color space and NV12/P010 fast-paths.

#include "importer/pixel_format_hdr.hpp"
#include "openmedia/core_types.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace openmedia::importer {

/// @brief Supported YUV-to-RGB color conversion matrices for frame scaling.
enum class ColorMatrix : uint8_t {
    Rec601 = 0,
    Rec709,
    Rec2020
};

/// @brief Configuration key used to cache and reuse SwsContext instances.
struct ScaleConfig {
    int32_t srcWidth = 0;
    int32_t srcHeight = 0;
    openmedia::PixelFormat srcFormat = openmedia::PixelFormat::Unknown;
    int32_t dstWidth = 0;
    int32_t dstHeight = 0;
    openmedia::PixelFormat dstFormat = openmedia::PixelFormat::BGRA8;
    ColorMatrix colorMatrix = ColorMatrix::Rec709;
    openmedia::ColorRange colorRange = openmedia::ColorRange::Limited;

    bool operator==(const ScaleConfig& other) const noexcept {
        return srcWidth == other.srcWidth &&
               srcHeight == other.srcHeight &&
               srcFormat == other.srcFormat &&
               dstWidth == other.dstWidth &&
               dstHeight == other.dstHeight &&
               dstFormat == other.dstFormat &&
               colorMatrix == other.colorMatrix &&
               colorRange == other.colorRange;
    }

    bool operator!=(const ScaleConfig& other) const noexcept {
        return !(*this == other);
    }
};

/// @brief Video frame scaler caching SwsContext across consecutive identical frames.
class FrameScaler {
public:
    FrameScaler();
    ~FrameScaler();

    FrameScaler(const FrameScaler&) = delete;
    FrameScaler& operator=(const FrameScaler&) = delete;
    FrameScaler(FrameScaler&& other) noexcept;
    FrameScaler& operator=(FrameScaler&& other) noexcept;

    openmedia::CoreStatus scale(const ScaleConfig& config,
                                const uint8_t* const srcPlanes[4],
                                const int32_t srcStrides[4],
                                uint8_t* const dstPlanes[4],
                                const int32_t dstStrides[4]) noexcept;

    openmedia::CoreStatus scaleFrame(const openmedia::RawVideoFrame& srcFrame,
                                     uint8_t* dstBuffer,
                                     int32_t dstStride,
                                     openmedia::PixelFormat dstFormat,
                                     int32_t dstWidth = 0,
                                     int32_t dstHeight = 0) noexcept;

    void reset() noexcept;
    uint64_t cacheMissCount() const noexcept;
    uint64_t scaleCount() const noexcept;
    ColorMatrix activeColorMatrix() const noexcept;

    static ColorMatrix mapColorSpace(openmedia::ColorSpace space) noexcept;
    static ColorMatrix mapExtendedColorSpace(ExtendedColorSpace space) noexcept;
    static int toSwsColorspaceId(ColorMatrix matrix) noexcept;
    static std::array<int, 4> getMatrixCoefficients(ColorMatrix matrix) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace openmedia::importer
