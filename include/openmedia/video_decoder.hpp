#pragma once
/// @file video_decoder.hpp
/// @brief Pimpl-encapsulated video decoder implementing ICoreVideoDecoder for AV1 and VP9.
/// @note Zero FFmpeg headers and zero proprietary Adobe Premiere Pro SDK (PrSDK*.h) headers are included here.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "openmedia/core_types.hpp"

namespace openmedia {

/// @brief Hardware video decode acceleration backend.
enum class HwDecodeBackend {
    None = 0,     ///< Software CPU decoding (libdav1d / libvpx-vp9).
    NVDEC,        ///< NVIDIA NVDEC hardware decode.
    D3D11VA,      ///< Windows Direct3D 11 Video Acceleration.
    VideoToolbox  ///< Apple Silicon / macOS VideoToolbox hardware decode.
};

/// @brief Concrete video decoder supporting AV1 (libdav1d) and VP9 (libvpx-vp9) with hardware decode hooks.
/// @details Uses the Pimpl idiom to isolate FFmpeg libavcodec/libavutil types from the public ABI,
///          produces 16-byte row-aligned RawVideoFrame buffers for 8-, 10-, and 12-bit YUV output,
///          normalizes non-zero start timestamps, and supports safe hardware-to-CPU fallback draining.
class CoreVideoDecoder final : public ICoreVideoDecoder {
public:
    /// @brief Constructs an uninitialized CoreVideoDecoder instance.
    CoreVideoDecoder();

    /// @brief Destroys the CoreVideoDecoder and releases all underlying decoder resources.
    ~CoreVideoDecoder() override;

    /// @brief Deleted copy constructor to enforce unique ownership of decoder state.
    CoreVideoDecoder(const CoreVideoDecoder&) = delete;

    /// @brief Deleted copy assignment operator to enforce unique ownership of decoder state.
    CoreVideoDecoder& operator=(const CoreVideoDecoder&) = delete;

    /// @brief Move constructor transferring ownership of internal decoder state.
    /// @param other Source decoder instance to move from.
    CoreVideoDecoder(CoreVideoDecoder&& other) noexcept;

    /// @brief Move assignment operator transferring ownership of internal decoder state.
    /// @param other Source decoder instance to move from.
    /// @return Reference to this decoder instance.
    CoreVideoDecoder& operator=(CoreVideoDecoder&& other) noexcept;

    /// @brief Initializes the video decoder in software CPU mode from container stream metadata.
    /// @param stream Stream descriptor specifying codec, dimensions, and optional extradata.
    /// @return CoreStatus::Ok on success, CoreStatus::InvalidArgument for non-video streams,
    ///         CoreStatus::UnsupportedCodec for unsupported codecs, or CoreStatus::OpenFailed on failure.
    CoreStatus initialize(const StreamInfo& stream) override;

    /// @brief Initializes the video decoder with a preferred hardware decode backend.
    /// @param stream Stream descriptor specifying codec, dimensions, and optional extradata.
    /// @param preferredBackend Preferred hardware acceleration backend (falls back to CPU if unavailable).
    /// @return CoreStatus::Ok on success or error status on invalid/unsupported stream.
    CoreStatus initializeWithHardware(const StreamInfo& stream, HwDecodeBackend preferredBackend);

    /// @brief Submits a compressed video packet to the underlying decoder.
    /// @param packet Compressed bitstream packet with non-empty payload.
    /// @return CoreStatus::Ok on success, CoreStatus::NotInitialized if uninitialized,
    ///         CoreStatus::InvalidArgument if payload is empty, CoreStatus::NeedMoreData if decoder
    ///         output buffer is full, or CoreStatus::DecodeFailed on bitstream errors.
    CoreStatus sendPacket(const EncodedPacket& packet) override;

    /// @brief Retrieves the next decoded, 16-byte row-aligned RawVideoFrame.
    /// @param outFrame Output frame populated with aligned plane buffers and color metadata.
    /// @return CoreStatus::Ok when a frame is produced, CoreStatus::NeedMoreData when more input
    ///         packets are required, CoreStatus::EndOfFile after drain completion,
    ///         CoreStatus::NoDecodableFrames if EOF is reached with zero decoded frames, or
    ///         CoreStatus::NotInitialized / CoreStatus::DecodeFailed on error.
    CoreStatus decodeNextFrame(RawVideoFrame& outFrame) override;

    /// @brief Signals end-of-stream so buffered frames can be drained via decodeNextFrame().
    /// @return CoreStatus::Ok on success, CoreStatus::NotInitialized if uninitialized,
    ///         or CoreStatus::DecodeFailed on error.
    CoreStatus flush() override;

    /// @brief Resets internal decoder buffers and state (e.g., after seeking).
    void reset() override;

    /// @brief Sets or resets the first-timestamp normalization baseline.
    /// @param startTimePts Explicit positive container start_time offset, or 0 to auto-capture frame 0 PTS.
    void resetFirstTimestamp(int64_t startTimePts = 0) noexcept;

    /// @brief Safely drains buffered frames and resets hardware context before falling back to CPU.
    /// @param drainedFrames Output vector populated with any frames drained from the hardware decoder.
    /// @return CoreStatus::Ok on successful drain and fallback, or CoreStatus::NotInitialized / error.
    CoreStatus drainBeforeFallback(std::vector<RawVideoFrame>& drainedFrames);

    /// @brief Returns the currently active decode backend.
    HwDecodeBackend activeBackend() const noexcept;

    /// @brief Returns true if a hardware decode backend is currently active.
    bool isHardwareActive() const noexcept;

    /// @brief Returns the latest diagnostic message (e.g., on NoDecodableFrames).
    const std::string& lastDiagnosticMessage() const noexcept;

private:
    /// @brief Opaque implementation structure holding FFmpeg RAII handles.
    struct Impl;

    /// @brief Pointer to the private implementation.
    std::unique_ptr<Impl> impl_;
};

} // namespace openmedia
