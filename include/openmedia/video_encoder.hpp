#pragma once
/// @file video_encoder.hpp
/// @brief Thread-safe RAII video encoder implementing ICoreVideoEncoder for AV1 and VP9.
/// @note Zero FFmpeg headers and zero Adobe Premiere SDK (PrSDK*.h) headers are included here.

#include "openmedia/core_types.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace openmedia {

/// @brief Concrete FFmpeg-backed video encoder supporting AV1 (libsvtav1 / av1_nvenc) and VP9 (libvpx-vp9).
/// @details Uses the Pimpl idiom to isolate all FFmpeg types from the public ABI boundary,
///          enforces 16-byte aligned frame buffers, and drains delayed GOP packets on flush().
class CoreVideoEncoder final : public ICoreVideoEncoder {
public:
    /// @brief Constructs an uninitialized CoreVideoEncoder instance.
    CoreVideoEncoder();

    /// @brief Destroys the encoder and releases all underlying FFmpeg resources via RAII.
    ~CoreVideoEncoder() override;

    CoreVideoEncoder(const CoreVideoEncoder&) = delete;
    CoreVideoEncoder& operator=(const CoreVideoEncoder&) = delete;

    /// @brief Move-constructs a CoreVideoEncoder, transferring ownership of the internal state.
    CoreVideoEncoder(CoreVideoEncoder&&) noexcept;

    /// @brief Move-assigns a CoreVideoEncoder, transferring ownership of the internal state.
    CoreVideoEncoder& operator=(CoreVideoEncoder&&) noexcept;

    /// @brief Initializes the video encoder with the specified codec, dimensions, pixel format, and rate control.
    /// @param config Encoding configuration (codec, resolution, rational framerate/timebase, CRF, GOP size).
    /// @return CoreStatus::Ok on success, CoreStatus::InvalidArgument on invalid parameters,
    ///         CoreStatus::UnsupportedCodec for unsupported codecs, or CoreStatus::OpenFailed if FFmpeg open fails.
    CoreStatus initialize(const VideoEncodeConfig& config) override;

    /// @brief Encodes a single raw video frame and appends any ready compressed packets to outPackets.
    /// @param frame Input RawVideoFrame matching the initialized dimensions and pixel format.
    /// @param outPackets Output vector receiving newly produced EncodedPacket instances.
    /// @return CoreStatus::Ok on success, CoreStatus::NotInitialized if uninitialized,
    ///         CoreStatus::InvalidArgument on frame mismatch, or CoreStatus::EncodeFailed on encoder error.
    CoreStatus encodeFrame(const RawVideoFrame& frame, std::vector<EncodedPacket>& outPackets) override;

    /// @brief Flushes delayed GOP frames at end-of-stream and appends remaining packets to outPackets.
    /// @param outPackets Output vector receiving all drained tail EncodedPacket instances.
    /// @return CoreStatus::Ok on success (idempotent if already flushed),
    ///         CoreStatus::NotInitialized if uninitialized, or CoreStatus::EncodeFailed on flush error.
    CoreStatus flush(std::vector<EncodedPacket>& outPackets) override;

    /// @brief Returns codec extradata / sequence headers generated during encoder initialization.
    /// @return Byte vector containing codec extradata, or empty if none was emitted.
    std::vector<uint8_t> getExtradata() const override;

    /// @brief Maps a normalized CRF value (0..63) to AV1 base_q_index (0..255).
    /// @param crf Input CRF value; values outside [0, 63] are clamped.
    /// @return Linearly scaled AV1 base_q_index in [0, 255].
    static constexpr int32_t normalizeCrfToBaseQIndex(int32_t crf) noexcept {
        const int32_t clamped = std::clamp(crf, 0, 63);
        return (clamped * 255 + 31) / 63;
    }

    /// @brief Maps a normalized CRF value (0..63) to VP9 / NVENC CQ level (0..51).
    /// @param crf Input CRF value; values outside [0, 63] are clamped.
    /// @return Linearly scaled CQ value in [0, 51].
    static constexpr int32_t normalizeCrfToVp9Cq(int32_t crf) noexcept {
        const int32_t clamped = std::clamp(crf, 0, 63);
        return (clamped * 51 + 31) / 63;
    }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace openmedia
