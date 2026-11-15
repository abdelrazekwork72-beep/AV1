#pragma once
/// @file core_types.hpp
/// @brief Shared ABI-safe C++17 data structures and pure virtual interfaces for OpenMediaCore.
/// @note Zero FFmpeg headers and zero proprietary Adobe SDK headers may be included here.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace openmedia {

/// @brief Rational timebase or frame-rate representation with safe denominator sanitization.
struct RationalTime {
    int64_t num = 0; ///< Numerator.
    int64_t den = 1; ///< Denominator (sanitized to >= 1 in arithmetic).

    /// @brief Returns a sanitized RationalTime ensuring denominator is strictly positive.
    static constexpr RationalTime sanitize(int64_t n, int64_t d) noexcept {
        return RationalTime{n, (d <= 0) ? 1 : d};
    }

    /// @brief Converts the rational value to floating-point seconds.
    double toSeconds() const noexcept {
        const int64_t safeDen = (den <= 0) ? 1 : den;
        return static_cast<double>(num) / static_cast<double>(safeDen);
    }
};

/// @brief High-level media stream category.
enum class MediaType {
    Unknown = 0,
    Video,
    Audio,
    Subtitle
};

/// @brief Supported modern video, audio, and subtitle codecs.
enum class CodecId {
    Unknown = 0,
    AV1,
    VP9,
    VP8,
    H264,
    HEVC,
    ProRes,
    FFV1,
    Opus,
    FLAC,
    Vorbis,
    PCM,
    SRT,
    WebVTT,
    XHE_AAC,
    AAC,
    MP3,
    AC3,
    DTS
};

/// @brief Supported pixel formats for raw decoded/encoded video frames.
enum class PixelFormat {
    Unknown = 0,
    YUV420P,
    YUV422P,
    YUV444P,
    YUV420P10LE,
    YUV422P10LE,
    YUV444P10LE,
    YUV420P12LE,
    YUV422P12LE,
    YUV444P12LE,
    BGRA8,
    RGBA8,
    NV12,
    P010LE
};

/// @brief Video sample range (limited vs full).
enum class ColorRange {
    Unspecified = 0,
    Limited,
    Full
};

/// @brief Video color primaries / matrix space.
enum class ColorSpace {
    Unspecified = 0,
    BT709,
    BT2020_NCL,
    BT2020_CL,
    BT601
};

/// @brief Video transfer characteristics (SDR / HDR).
enum class ColorTransfer {
    Unspecified = 0,
    BT709,
    PQ_ST2084,
    HLG,
    sRGB
};

/// @brief Unified status codes returned across OpenMediaCore interfaces.
enum class CoreStatus {
    Ok = 0,
    EndOfFile,
    NeedMoreData,
    InvalidArgument,
    OpenFailed,
    DecodeFailed,
    EncodeFailed,
    SeekFailed,
    UnsupportedCodec,
    NotInitialized,
    NoDecodableFrames
};

/// @brief Metadata describing a single container stream.
struct StreamInfo {
    int32_t index = -1;                              ///< Zero-based stream index.
    MediaType type = MediaType::Unknown;             ///< Stream media type.
    CodecId codec = CodecId::Unknown;                ///< Stream codec identifier.
    RationalTime timeBase{1, 1000};                  ///< Stream timebase.
    RationalTime frameRate{0, 1};                    ///< Average or nominal frame rate.
    int64_t durationPts = 0;                         ///< Stream duration in timeBase units.
    int32_t width = 0;                               ///< Video width in pixels.
    int32_t height = 0;                              ///< Video height in pixels.
    int32_t bitDepth = 8;                            ///< Bits per component (8, 10, or 12).
    PixelFormat pixelFormat = PixelFormat::Unknown;  ///< Video pixel format.
    int32_t sampleRate = 0;                          ///< Audio sample rate in Hz.
    int32_t channels = 0;                            ///< Audio channel count.
    std::string language;                            ///< ISO-639 language tag if present.
    std::vector<uint8_t> extradata;                  ///< Codec-specific extradata / sequence headers.
};

/// @brief Compressed media packet passed between demuxer, decoders, encoders, and muxer.
struct EncodedPacket {
    int32_t streamIndex = 0;                         ///< Target or source stream index.
    int64_t pts = 0;                                 ///< Presentation timestamp.
    int64_t dts = 0;                                 ///< Decoding timestamp.
    int64_t duration = 0;                            ///< Packet duration in stream timebase.
    bool isKeyFrame = false;                         ///< True if packet is a random-access keyframe.
    std::vector<uint8_t> data;                       ///< Compressed bitstream payload.
};

/// @brief Decoded or pre-encode planar/packed video frame with 16-byte aligned row strides.
struct RawVideoFrame {
    int32_t width = 0;                                  ///< Frame width in pixels.
    int32_t height = 0;                                 ///< Frame height in pixels.
    int32_t bitDepth = 8;                               ///< Bits per component (8, 10, 12).
    PixelFormat format = PixelFormat::Unknown;          ///< Pixel format layout.
    ColorRange colorRange = ColorRange::Limited;        ///< Luma/chroma range.
    ColorSpace colorSpace = ColorSpace::BT709;          ///< Color matrix space.
    ColorTransfer colorTransfer = ColorTransfer::BT709; ///< Transfer function.
    int64_t pts = 0;                                    ///< Presentation timestamp.
    int64_t duration = 0;                               ///< Frame duration.
    bool isKeyFrame = false;                            ///< True if decoded from keyframe.
    std::array<uint32_t, 4> rowBytes{0, 0, 0, 0};       ///< 16-byte aligned stride per plane.
    std::array<std::vector<uint8_t>, 4> planes;         ///< Plane pixel buffers.

    /// @brief Rounds a byte width up to the nearest 16-byte boundary.
    static constexpr uint32_t alignRowStride16(uint32_t widthBytes) noexcept {
        return ((widthBytes + 15U) / 16U) * 16U;
    }
};

/// @brief Decoded or pre-encode 32-bit floating-point planar audio batch ([-1.0f, 1.0f]).
struct RawAudioBatch {
    int32_t sampleRate = 48000;                         ///< Audio sample rate in Hz.
    int32_t channels = 2;                               ///< Number of planar channels.
    int32_t numSamples = 0;                             ///< Samples per channel in this batch.
    int64_t pts = 0;                                    ///< Batch presentation timestamp.
    std::vector<std::vector<float>> channelData;        ///< Planar float buffers [channel][sample].
};

/// @brief Configuration for CoreVideoEncoder initialization.
struct VideoEncodeConfig {
    CodecId codec = CodecId::AV1;                       ///< Target video codec.
    int32_t width = 1920;                               ///< Output frame width.
    int32_t height = 1080;                              ///< Output frame height.
    RationalTime frameRate{30, 1};                      ///< Output frame rate.
    RationalTime timeBase{1, 30};                       ///< Output stream timebase.
    PixelFormat pixelFormat = PixelFormat::YUV420P10LE; ///< Input/output pixel format.
    int32_t bitDepth = 10;                              ///< Target bit depth (8, 10, 12).
    int32_t crf = 28;                                   ///< Normalized CRF (0..63).
    int32_t preset = 6;                                 ///< Speed/quality preset.
    int32_t gopSize = 60;                               ///< Maximum keyframe interval.
    int64_t bitrateBps = 0;                             ///< Optional target bitrate (0 = CRF).
    bool preferHardware = false;                        ///< Prefer GPU hardware encoder if available.
};

/// @brief Configuration for CoreAudioEncoder initialization.
struct AudioEncodeConfig {
    CodecId codec = CodecId::Opus;                      ///< Target audio codec (Opus / FLAC).
    int32_t sampleRate = 48000;                         ///< Output sample rate in Hz.
    int32_t channels = 2;                               ///< Output channel count.
    int64_t bitrateBps = 128000;                        ///< Target bitrate in bps (lossy codecs).
};

/// @brief Stream descriptor passed to CoreMuxer::addStream().
struct MuxerStreamConfig {
    StreamInfo info;                                    ///< Stream metadata and extradata.
};

/// @brief Abstract interface for container demuxing and stream discovery.
class ICoreDemuxer {
public:
    virtual ~ICoreDemuxer() = default;
    /// @brief Opens a media container file and discovers all streams.
    virtual CoreStatus openFile(const std::string& path) = 0;
    /// @brief Releases OS file handles while preserving stream metadata for fast reopen.
    virtual CoreStatus quietFile() = 0;
    /// @brief Reopens the media file if it was previously quieted.
    virtual CoreStatus reopenIfQuieted() = 0;
    /// @brief Closes the demuxer and releases all resources.
    virtual void close() = 0;
    /// @brief Returns discovered stream metadata.
    virtual const std::vector<StreamInfo>& getStreams() const = 0;
    /// @brief Reads the next compressed packet from the container.
    virtual CoreStatus readNextPacket(EncodedPacket& outPacket) = 0;
    /// @brief Seeks to the nearest keyframe at or before timestampPts in the given stream.
    virtual CoreStatus seekToTimestamp(int32_t streamIndex, int64_t timestampPts) = 0;
};

/// @brief Abstract interface for hardware/software video frame decoding.
class ICoreVideoDecoder {
public:
    virtual ~ICoreVideoDecoder() = default;
    /// @brief Initializes the video decoder from stream metadata.
    virtual CoreStatus initialize(const StreamInfo& stream) = 0;
    /// @brief Submits a compressed video packet to the decoder.
    virtual CoreStatus sendPacket(const EncodedPacket& packet) = 0;
    /// @brief Retrieves the next decoded 16-byte aligned RawVideoFrame.
    virtual CoreStatus decodeNextFrame(RawVideoFrame& outFrame) = 0;
    /// @brief Signals end-of-stream to drain buffered frames.
    virtual CoreStatus flush() = 0;
    /// @brief Resets internal decoder buffers (e.g., after seeking).
    virtual void reset() = 0;
};

/// @brief Abstract interface for 32-bit float planar audio decoding and resampling.
class ICoreAudioDecoder {
public:
    virtual ~ICoreAudioDecoder() = default;
    /// @brief Initializes the audio decoder and resampler to targetSampleRate.
    virtual CoreStatus initialize(const StreamInfo& stream, int32_t targetSampleRate = 48000) = 0;
    /// @brief Submits a compressed audio packet to the decoder.
    virtual CoreStatus sendPacket(const EncodedPacket& packet) = 0;
    /// @brief Decodes and resamples up to maxSamples into 32-bit float planar output.
    virtual CoreStatus decodeAudioBatch(int32_t maxSamples, RawAudioBatch& outBatch) = 0;
    /// @brief Signals end-of-stream to drain buffered audio frames.
    virtual CoreStatus flush() = 0;
    /// @brief Resets decoder and resampler state.
    virtual void reset() = 0;
};

/// @brief Abstract interface for video frame encoding.
class ICoreVideoEncoder {
public:
    virtual ~ICoreVideoEncoder() = default;
    /// @brief Initializes the video encoder with the given configuration.
    virtual CoreStatus initialize(const VideoEncodeConfig& config) = 0;
    /// @brief Encodes a raw video frame and appends any ready packets to outPackets.
    virtual CoreStatus encodeFrame(const RawVideoFrame& frame, std::vector<EncodedPacket>& outPackets) = 0;
    /// @brief Flushes delayed GOP frames at end-of-stream.
    virtual CoreStatus flush(std::vector<EncodedPacket>& outPackets) = 0;
    /// @brief Returns codec extradata / sequence headers generated by the encoder.
    virtual std::vector<uint8_t> getExtradata() const = 0;
};

/// @brief Abstract interface for 32-bit float planar audio encoding.
class ICoreAudioEncoder {
public:
    virtual ~ICoreAudioEncoder() = default;
    /// @brief Initializes the audio encoder with the given configuration.
    virtual CoreStatus initialize(const AudioEncodeConfig& config) = 0;
    /// @brief Encodes a planar float audio batch and appends output packets.
    virtual CoreStatus encodeAudio(const RawAudioBatch& batch, std::vector<EncodedPacket>& outPackets) = 0;
    /// @brief Flushes any remaining buffered audio samples.
    virtual CoreStatus flush(std::vector<EncodedPacket>& outPackets) = 0;
    /// @brief Returns codec extradata (e.g., OpusHead) generated by the encoder.
    virtual std::vector<uint8_t> getExtradata() const = 0;
};

/// @brief Abstract interface for interleaved container multiplexing.
class ICoreMuxer {
public:
    virtual ~ICoreMuxer() = default;
    /// @brief Opens an output container for the specified format (e.g., matroska, webm, mp4).
    virtual CoreStatus openContainer(const std::string& path, const std::string& formatName) = 0;
    /// @brief Adds a video, audio, or subtitle stream and returns its assigned stream index.
    virtual int32_t addStream(const MuxerStreamConfig& streamConfig) = 0;
    /// @brief Writes the container header after all streams are added.
    virtual CoreStatus writeHeader() = 0;
    /// @brief Writes a compressed packet using interleaved timestamp ordering.
    virtual CoreStatus writeInterleavedPacket(const EncodedPacket& packet) = 0;
    /// @brief Finalizes Cues/SeekHead trailer and closes the output container.
    virtual CoreStatus writeTrailerAndClose() = 0;
};

} // namespace openmedia
