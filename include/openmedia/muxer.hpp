#pragma once
/// @file muxer.hpp
/// @brief ABI-safe Pimpl declarations for CoreMuxer and CoreAudioEncoder.
/// @note Zero FFmpeg headers and zero PrSDK*.h headers may be included here.

#include "openmedia/core_types.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace openmedia {

/// @brief Concrete FFmpeg-backed container multiplexer implementing ICoreMuxer.
/// @details Uses the Pimpl idiom to isolate libavformat/libavcodec types from
///          downstream Premiere Pro SDK translation units.
class CoreMuxer final : public ICoreMuxer {
public:
    /// @brief Constructs an uninitialized CoreMuxer instance.
    CoreMuxer();

    /// @brief Destroys the CoreMuxer and releases any open container resources.
    ~CoreMuxer() override;

    CoreMuxer(const CoreMuxer&) = delete;
    CoreMuxer& operator=(const CoreMuxer&) = delete;

    /// @brief Move-constructs a CoreMuxer transferring ownership of internal state.
    CoreMuxer(CoreMuxer&&) noexcept;

    /// @brief Move-assigns a CoreMuxer transferring ownership of internal state.
    CoreMuxer& operator=(CoreMuxer&&) noexcept;

    /// @brief Opens an output container for the specified format (matroska, webm, or mp4).
    /// @param path Destination filesystem path for the multiplexed container.
    /// @param formatName FFmpeg muxer short name ("matroska", "webm", or "mp4").
    /// @return CoreStatus::Ok on success, InvalidArgument on bad inputs, or OpenFailed on I/O error.
    CoreStatus openContainer(const std::string& path, const std::string& formatName) override;

    /// @brief Registers a video, audio, or subtitle stream in the open container.
    /// @param streamConfig Stream metadata, codec parameters, and optional extradata.
    /// @return Zero-based assigned stream index on success, or -1 on failure.
    int32_t addStream(const MuxerStreamConfig& streamConfig) override;

    /// @brief Writes the container header after all streams have been registered.
    /// @return CoreStatus::Ok on success, NotInitialized if not open, or InvalidArgument if no streams exist.
    CoreStatus writeHeader() override;

    /// @brief Writes a compressed packet using interleaved timestamp ordering.
    /// @param packet Compressed packet with valid streamIndex and non-decreasing DTS per stream.
    /// @return CoreStatus::Ok on success, NotInitialized if header not written, or InvalidArgument on invalid packet/order.
    CoreStatus writeInterleavedPacket(const EncodedPacket& packet) override;

    /// @brief Finalizes container Cues/SeekHead trailer and closes the output file.
    /// @return CoreStatus::Ok on success or NotInitialized if container was not opened.
    CoreStatus writeTrailerAndClose() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_; ///< Opaque Pimpl pointer hiding FFmpeg muxer state.
};

/// @brief Concrete FFmpeg-backed audio encoder implementing ICoreAudioEncoder.
/// @details Supports Opus (libopus) and FLAC (libflac/flac) encoding from 32-bit
///          floating-point planar RawAudioBatch inputs.
class CoreAudioEncoder final : public ICoreAudioEncoder {
public:
    /// @brief Constructs an uninitialized CoreAudioEncoder instance.
    CoreAudioEncoder();

    /// @brief Destroys the CoreAudioEncoder and frees FFmpeg codec resources.
    ~CoreAudioEncoder() override;

    CoreAudioEncoder(const CoreAudioEncoder&) = delete;
    CoreAudioEncoder& operator=(const CoreAudioEncoder&) = delete;

    /// @brief Move-constructs a CoreAudioEncoder transferring ownership of internal state.
    CoreAudioEncoder(CoreAudioEncoder&&) noexcept;

    /// @brief Move-assigns a CoreAudioEncoder transferring ownership of internal state.
    CoreAudioEncoder& operator=(CoreAudioEncoder&&) noexcept;

    /// @brief Initializes the audio encoder with the requested codec and stream parameters.
    /// @param config Audio encoding configuration (CodecId::Opus or CodecId::FLAC).
    /// @return CoreStatus::Ok on success, UnsupportedCodec, InvalidArgument, or OpenFailed.
    CoreStatus initialize(const AudioEncodeConfig& config) override;

    /// @brief Encodes a 32-bit float planar audio batch and appends ready packets.
    /// @param batch Planar float audio samples in [-1.0f, 1.0f].
    /// @param outPackets Output vector to which encoded packets are appended.
    /// @return CoreStatus::Ok on success, NotInitialized, InvalidArgument, or EncodeFailed.
    CoreStatus encodeAudio(const RawAudioBatch& batch, std::vector<EncodedPacket>& outPackets) override;

    /// @brief Flushes any buffered samples and delayed packets at end-of-stream.
    /// @param outPackets Output vector to which flushed packets are appended.
    /// @return CoreStatus::Ok on success, NotInitialized, or EncodeFailed.
    CoreStatus flush(std::vector<EncodedPacket>& outPackets) override;

    /// @brief Returns codec-specific extradata (e.g., OpusHead) generated during initialization.
    /// @return Byte vector containing extradata, or empty if none.
    std::vector<uint8_t> getExtradata() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_; ///< Opaque Pimpl pointer hiding FFmpeg encoder state.
};

} // namespace openmedia
