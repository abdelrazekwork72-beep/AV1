#pragma once
/// @file frame_scaler.hpp
/// @brief Reusable SwsContext video frame scaler with Rec.601/709/2020 color matrix selection.

#include "openmedia/core_types.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace openmedia::importer {

/// @brief Supported YUV-to-RGB color conversion matrices for frame scaling.
enum class ColorMatrix : uint8_t {
    Rec601 = 0, ///< ITU-R BT.601 standard definition matrix.
    Rec709,     ///< ITU-R BT.709 high definition matrix.
    Rec2020     ///< ITU-R BT.2020 ultra-high definition matrix.
};

/// @brief Configuration key used to cache and reuse SwsContext instances.
struct ScaleConfig {
    int32_t srcWidth = 0;                                         ///< Source frame width in pixels.
    int32_t srcHeight = 0;                                        ///< Source frame height in pixels.
    openmedia::PixelFormat srcFormat = openmedia::PixelFormat::Unknown; ///< Source pixel format.
    int32_t dstWidth = 0;                                         ///< Destination frame width in pixels.
    int32_t dstHeight = 0;                                        ///< Destination frame height in pixels.
    openmedia::PixelFormat dstFormat = openmedia::PixelFormat::BGRA8;   ///< Destination pixel format.
    ColorMatrix colorMatrix = ColorMatrix::Rec709;                ///< Color conversion matrix.
    openmedia::ColorRange colorRange = openmedia::ColorRange::Limited;  ///< Luma/chroma sample range.

    /// @brief Equality comparison for context cache key matching.
    /// @param other Configuration to compare against.
    /// @return True if all scaling parameters match.
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

    /// @brief Inequality comparison for context cache key matching.
    /// @param other Configuration to compare against.
    /// @return True if any scaling parameter differs.
    bool operator!=(const ScaleConfig& other) const noexcept {
        return !(*this == other);
    }
};

/// @brief Video frame scaler caching SwsContext across consecutive identical frames.
/// @details Wraps libswscale (`sws_getCachedContext` + `sws_setColorspaceDetails` + `sws_scale`)
///          under `#ifdef OPENMEDIA_HAS_FFMPEG`, with a deterministic C++17 fallback when FFmpeg
///          headers are unavailable. Always passes `dstStride` directly without recomputation.
class FrameScaler {
public:
    /// @brief Constructs a FrameScaler with an empty SwsContext cache.
    FrameScaler();

    /// @brief Destroys the FrameScaler and frees any cached SwsContext.
    ~FrameScaler();

    FrameScaler(const FrameScaler&) = delete;
    FrameScaler& operator=(const FrameScaler&) = delete;

    /// @brief Move-constructs a FrameScaler, transferring the cached context and counters.
    /// @param other Source scaler to move from.
    FrameScaler(FrameScaler&& other) noexcept;

    /// @brief Move-assigns a FrameScaler, transferring the cached context and counters.
    /// @param other Source scaler to move from.
    /// @return Reference to this scaler.
    FrameScaler& operator=(FrameScaler&& other) noexcept;

    /// @brief Scales and color-converts a frame using explicit plane pointers and strides.
    /// @param config Scaling dimensions, pixel formats, color matrix, and sample range.
    /// @param srcPlanes Array of 4 source plane pointers.
    /// @param srcStrides Array of 4 source row strides in bytes.
    /// @param dstPlanes Array of 4 destination plane pointers.
    /// @param dstStrides Array of 4 destination row strides in bytes (passed directly as dstStride).
    /// @return CoreStatus::Ok on success, or an error status on invalid input or failure.
    openmedia::CoreStatus scale(const ScaleConfig& config,
                                const uint8_t* const srcPlanes[4],
                                const int32_t srcStrides[4],
                                uint8_t* const dstPlanes[4],
                                const int32_t dstStrides[4]) noexcept;

    /// @brief Scales a RawVideoFrame into a locked destination buffer with an explicit stride.
    /// @param srcFrame Decoded source video frame.
    /// @param dstBuffer Destination pixel buffer pointer.
    /// @param dstStride Destination row stride in bytes (e.g., from PrSDKPPixSuite::GetRowBytes).
    /// @param dstFormat Target destination pixel format.
    /// @param dstWidth Optional target width (defaults to srcFrame.width when <= 0).
    /// @param dstHeight Optional target height (defaults to srcFrame.height when <= 0).
    /// @return CoreStatus::Ok on success, or an error status on invalid input.
    openmedia::CoreStatus scaleFrame(const openmedia::RawVideoFrame& srcFrame,
                                     uint8_t* dstBuffer,
                                     int32_t dstStride,
                                     openmedia::PixelFormat dstFormat,
                                     int32_t dstWidth = 0,
                                     int32_t dstHeight = 0) noexcept;

    /// @brief Releases the cached SwsContext and resets allocation/scale counters to zero.
    void reset() noexcept;

    /// @brief Returns the number of SwsContext allocations/reallocations performed.
    /// @return Cumulative cache miss count.
    uint64_t cacheMissCount() const noexcept;

    /// @brief Returns the total number of frames successfully scaled.
    /// @return Cumulative scaled frame count.
    uint64_t scaleCount() const noexcept;

    /// @brief Returns the color matrix applied by the most recent scale operation.
    /// @return Active ColorMatrix value.
    ColorMatrix activeColorMatrix() const noexcept;

    /// @brief Maps an openmedia::ColorSpace enum to the corresponding ColorMatrix.
    /// @param space Source stream color space.
    /// @return Mapped ColorMatrix (defaults to Rec709 when unspecified).
    static ColorMatrix mapColorSpace(openmedia::ColorSpace space) noexcept;

    /// @brief Returns the libswscale SWS_CS_* identifier for a ColorMatrix.
    /// @param matrix Target color matrix.
    /// @return Integer SWS_CS_* constant (ITU601=5, ITU709=1, BT2020=9).
    static int toSwsColorspaceId(ColorMatrix matrix) noexcept;

    /// @brief Returns the 4-element fixed-point ITU matrix coefficient tuple for a ColorMatrix.
    /// @param matrix Target color matrix.
    /// @return Array of 4 integer coefficients matching libswscale's sws_getCoefficients order.
    static std::array<int, 4> getMatrixCoefficients(ColorMatrix matrix) noexcept;

private:
    struct Impl; ///< Forward-declared Pimpl state holding cached SwsContext and counters.
    std::unique_ptr<Impl> impl_; ///< Owning pointer to private implementation.
};

} // namespace openmedia::importer
