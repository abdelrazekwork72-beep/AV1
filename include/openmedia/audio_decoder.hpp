#pragma once
/// @file audio_decoder.hpp
/// @brief Pimpl-encapsulated 32-bit float planar audio decoder interface for OpenMediaCore.
/// @note Contains zero FFmpeg headers and zero proprietary Adobe SDK headers.

#include <algorithm>
#include <cstdint>
#include <memory>

#include "openmedia/core_types.hpp"

namespace openmedia {

/// @brief Converts a signed 16-bit PCM sample to a clamped 32-bit float sample.
/// @details Computes Sample_32f = Sample_16i / 32767.0f and clamps the result
///          to [-1.0f, 1.0f] so INT16_MIN (-32768) maps cleanly to -1.0f.
/// @param sample Signed 16-bit integer PCM sample.
/// @return Normalized 32-bit floating-point sample clamped to [-1.0f, 1.0f].
inline float convertInt16ToFloat32(int16_t sample) noexcept {
    const float normalized = static_cast<float>(sample) / 32767.0f;
    return std::clamp(normalized, -1.0f, 1.0f);
}

/// @brief Concrete FFmpeg-backed audio decoder producing 32-bit float planar batches.
/// @details Supports Opus, FLAC, Vorbis, PCM (including 5-channel and 6-channel MOV/MKV
///          layouts), xHE-AAC, AAC, MP3, AC3, and DTS streams up to 32 channels,
///          resampling decoded output to AV_SAMPLE_FMT_FLTP via libswresample.
///          Tolerates isolated corrupted packets via 0.0f planar float silence substitution
///          and synthesizes planar float silence for zero-packet declared audio tracks.
class CoreAudioDecoder final : public ICoreAudioDecoder {
public:
    /// @brief Constructs an uninitialized CoreAudioDecoder instance.
    CoreAudioDecoder();

    /// @brief Destroys the CoreAudioDecoder and releases all internal codec/resampler resources.
    ~CoreAudioDecoder() override;

    CoreAudioDecoder(const CoreAudioDecoder&) = delete;
    CoreAudioDecoder& operator=(const CoreAudioDecoder&) = delete;

    /// @brief Move-constructs a CoreAudioDecoder transferring ownership of internal state.
    /// @param other Source decoder instance to move from.
    CoreAudioDecoder(CoreAudioDecoder&& other) noexcept;

    /// @brief Move-assigns a CoreAudioDecoder transferring ownership of internal state.
    /// @param other Source decoder instance to move from.
    /// @return Reference to this decoder instance.
    CoreAudioDecoder& operator=(CoreAudioDecoder&& other) noexcept;

    /// @brief Initializes the audio decoder and resampler for the specified stream.
    /// @param stream Metadata describing the compressed audio stream and extradata.
    /// @param targetSampleRate Desired output sample rate in Hz (default 48000).
    /// @return CoreStatus::Ok on success, or an error status on invalid input/unsupported codec.
    CoreStatus initialize(const StreamInfo& stream, int32_t targetSampleRate = 48000) override;

    /// @brief Submits a compressed audio packet to the decoder.
    /// @details Isolated corrupted packets are tolerated by substituting zero-filled
    ///          planar float silence (0.0f) rather than aborting the import.
    /// @param packet Compressed bitstream packet to decode.
    /// @return CoreStatus::Ok on success, CoreStatus::NeedMoreData if decoder buffers are full,
    ///         or an error status on uninitialized/invalid state.
    CoreStatus sendPacket(const EncodedPacket& packet) override;

    /// @brief Decodes and resamples up to maxSamples frames into 32-bit float planar output.
    /// @param maxSamples Maximum number of samples per channel to return in outBatch.
    /// @param outBatch Destination batch populated with planar float channel buffers.
    /// @return CoreStatus::Ok if a batch was produced, CoreStatus::NeedMoreData if more packets
    ///         are required, CoreStatus::EndOfFile when drained, or an error status.
    CoreStatus decodeAudioBatch(int32_t maxSamples, RawAudioBatch& outBatch) override;

    /// @brief Signals end-of-stream to drain buffered decoder and resampler samples.
    /// @details If zero packets were submitted for the declared audio track, synthesizes
    ///          planar float silence (0.0f) so empty tracks do not fail import.
    /// @return CoreStatus::Ok on success, or an error status if flushing fails.
    CoreStatus flush() override;

    /// @brief Resets internal decoder and resampler state (e.g., after seeking).
    void reset() override;

private:
    /// @brief Opaque implementation class encapsulating FFmpeg and libswresample state.
    class Impl;
    std::unique_ptr<Impl> impl_; ///< Pointer to private implementation.
};

} // namespace openmedia
