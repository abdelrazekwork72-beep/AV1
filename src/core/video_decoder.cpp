/// @file video_decoder.cpp
/// @brief Implementation of CoreVideoDecoder using FFmpeg libavcodec (libdav1d / libvpx-vp9)
///        with EOF draining, non-zero PTS normalization, zero-frame guard, and HW fallback hooks.

#include "openmedia/video_decoder.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <deque>
#include <limits>
#include <utility>

#ifdef OPENMEDIA_HAS_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/buffer.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>
}
#endif

namespace openmedia {

namespace {

constexpr const char* kNoDecodableFramesMsg = "No decodable video frames in file";

#ifdef OPENMEDIA_HAS_FFMPEG

/// @brief RAII custom deleter for AVCodecContext.
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const noexcept {
        if (ctx != nullptr) {
            avcodec_free_context(&ctx);
        }
    }
};

/// @brief RAII custom deleter for AVFrame.
struct AVFrameDeleter {
    void operator()(AVFrame* frame) const noexcept {
        if (frame != nullptr) {
            av_frame_free(&frame);
        }
    }
};

/// @brief RAII custom deleter for AVPacket.
struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const noexcept {
        if (pkt != nullptr) {
            av_packet_free(&pkt);
        }
    }
};

/// @brief RAII custom deleter for AVBufferRef.
struct AVBufferRefDeleter {
    void operator()(AVBufferRef* ref) const noexcept {
        if (ref != nullptr) {
            av_buffer_unref(&ref);
        }
    }
};

using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
using AVBufferRefPtr = std::unique_ptr<AVBufferRef, AVBufferRefDeleter>;

/// @brief Static mapping entry from OpenMedia CodecId to FFmpeg decoder name and AVCodecID.
struct CodecMapEntry {
    CodecId codecId;
    const char* decoderName;
    AVCodecID avCodecId;
};

/// @brief Supported software video decoders (all Influx v1.7.1 + v2.0.0beta1 codecs).
constexpr std::array<CodecMapEntry, 16> kCodecTable{{
    {CodecId::AV1, "libdav1d", AV_CODEC_ID_AV1},
    {CodecId::VP9, "libvpx-vp9", AV_CODEC_ID_VP9},
    {CodecId::VP8, "libvpx", AV_CODEC_ID_VP8},
    {CodecId::H264, "h264", AV_CODEC_ID_H264},
    {CodecId::HEVC, "hevc", AV_CODEC_ID_HEVC},
    {CodecId::VVC, "vvc", AV_CODEC_ID_VVC},
    {CodecId::ProRes, "prores", AV_CODEC_ID_PRORES},
    {CodecId::FFV1, "ffv1", AV_CODEC_ID_FFV1},
    {CodecId::HAP, "hap", AV_CODEC_ID_HAP},
    {CodecId::DXV, "dxv", AV_CODEC_ID_DXV},
    {CodecId::Canopus, "cfhd", AV_CODEC_ID_CFHD},
    {CodecId::Bink, "binkvideo", AV_CODEC_ID_BINKVIDEO},
    {CodecId::MJPEG, "mjpeg", AV_CODEC_ID_MJPEG},
    {CodecId::QOI, "qoi", AV_CODEC_ID_QOI},
    {CodecId::WebP, "webp", AV_CODEC_ID_WEBP},
    {CodecId::APNG, "apng", AV_CODEC_ID_APNG},
}};

/// @brief Static mapping entry for hardware decode backends.
struct HwBackendEntry {
    HwDecodeBackend backend;
    AVHWDeviceType avType;
    const char* name;
};

/// @brief Lookup table for supported hardware decode contexts.
constexpr std::array<HwBackendEntry, 3> kHwBackendTable{{
    {HwDecodeBackend::NVDEC, AV_HWDEVICE_TYPE_CUDA, "nvdec"},
    {HwDecodeBackend::D3D11VA, AV_HWDEVICE_TYPE_D3D11VA, "d3d11va"},
    {HwDecodeBackend::VideoToolbox, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, "videotoolbox"},
}};

/// @brief Static mapping entry from AVPixelFormat to OpenMedia PixelFormat and plane geometry.
struct PixelFormatMapEntry {
    AVPixelFormat avFormat;
    PixelFormat omFormat;
    int32_t bitDepth;
    uint32_t numPlanes;
    uint32_t bytesPerSample;
    uint32_t chromaShiftX;
    uint32_t chromaShiftY;
    bool semiPlanar;
};

/// @brief Lookup table for 8-bit, 10-bit, and 12-bit YUV pixel formats.
constexpr std::array<PixelFormatMapEntry, 11> kPixelFormatTable{{
    {AV_PIX_FMT_YUV420P, PixelFormat::YUV420P, 8, 3, 1, 1, 1, false},
    {AV_PIX_FMT_YUV422P, PixelFormat::YUV422P, 8, 3, 1, 1, 0, false},
    {AV_PIX_FMT_YUV444P, PixelFormat::YUV444P, 8, 3, 1, 0, 0, false},
    {AV_PIX_FMT_YUV420P10LE, PixelFormat::YUV420P10LE, 10, 3, 2, 1, 1, false},
    {AV_PIX_FMT_YUV422P10LE, PixelFormat::YUV422P10LE, 10, 3, 2, 1, 0, false},
    {AV_PIX_FMT_YUV444P10LE, PixelFormat::YUV444P10LE, 10, 3, 2, 0, 0, false},
    {AV_PIX_FMT_YUV420P12LE, PixelFormat::YUV420P12LE, 12, 3, 2, 1, 1, false},
    {AV_PIX_FMT_YUV422P12LE, PixelFormat::YUV422P12LE, 12, 3, 2, 1, 0, false},
    {AV_PIX_FMT_YUV444P12LE, PixelFormat::YUV444P12LE, 12, 3, 2, 0, 0, false},
    {AV_PIX_FMT_NV12, PixelFormat::NV12, 8, 2, 1, 1, 1, true},
    {AV_PIX_FMT_P010LE, PixelFormat::P010LE, 10, 2, 2, 1, 1, true},
}};

/// @brief Static mapping entry for AVColorRange.
struct ColorRangeMapEntry {
    AVColorRange avRange;
    ColorRange omRange;
};

constexpr std::array<ColorRangeMapEntry, 2> kColorRangeTable{{
    {AVCOL_RANGE_MPEG, ColorRange::Limited},
    {AVCOL_RANGE_JPEG, ColorRange::Full},
}};

/// @brief Static mapping entry for AVColorSpace.
struct ColorSpaceMapEntry {
    AVColorSpace avSpace;
    ColorSpace omSpace;
};

constexpr std::array<ColorSpaceMapEntry, 5> kColorSpaceTable{{
    {AVCOL_SPC_BT709, ColorSpace::BT709},
    {AVCOL_SPC_BT2020_NCL, ColorSpace::BT2020_NCL},
    {AVCOL_SPC_BT2020_CL, ColorSpace::BT2020_CL},
    {AVCOL_SPC_BT470BG, ColorSpace::BT601},
    {AVCOL_SPC_SMPTE170M, ColorSpace::BT601},
}};

/// @brief Static mapping entry for AVColorTransferCharacteristic.
struct ColorTransferMapEntry {
    AVColorTransferCharacteristic avTrc;
    ColorTransfer omTrc;
};

constexpr std::array<ColorTransferMapEntry, 4> kColorTransferTable{{
    {AVCOL_TRC_BT709, ColorTransfer::BT709},
    {AVCOL_TRC_SMPTE2084, ColorTransfer::PQ_ST2084},
    {AVCOL_TRC_ARIB_STD_B67, ColorTransfer::HLG},
    {AVCOL_TRC_IEC61966_2_1, ColorTransfer::sRGB},
}};

/// @brief Looks up a CodecMapEntry for the given CodecId.
const CodecMapEntry* findCodecEntry(CodecId codec) noexcept {
    for (const auto& entry : kCodecTable) {
        if (entry.codecId == codec) {
            return &entry;
        }
    }
    return nullptr;
}

/// @brief Looks up hardware backend metadata for a HwDecodeBackend.
const HwBackendEntry* findHwBackendEntry(HwDecodeBackend backend) noexcept {
    for (const auto& entry : kHwBackendTable) {
        if (entry.backend == backend) {
            return &entry;
        }
    }
    return nullptr;
}

/// @brief Finds the FFmpeg AVCodec for a CodecMapEntry, preferring the named decoder.
const AVCodec* findFFmpegDecoder(const CodecMapEntry& entry) noexcept {
    const AVCodec* codec = avcodec_find_decoder_by_name(entry.decoderName);
    if (codec != nullptr) {
        return codec;
    }
    return avcodec_find_decoder(entry.avCodecId);
}

/// @brief Looks up pixel format geometry for an AVPixelFormat.
const PixelFormatMapEntry* findPixelFormatEntry(AVPixelFormat fmt) noexcept {
    for (const auto& entry : kPixelFormatTable) {
        if (entry.avFormat == fmt) {
            return &entry;
        }
    }
    return nullptr;
}

/// @brief Maps AVColorRange to OpenMedia ColorRange.
ColorRange mapColorRange(AVColorRange range) noexcept {
    for (const auto& entry : kColorRangeTable) {
        if (entry.avRange == range) {
            return entry.omRange;
        }
    }
    return ColorRange::Unspecified;
}

/// @brief Maps AVColorSpace to OpenMedia ColorSpace.
ColorSpace mapColorSpace(AVColorSpace space) noexcept {
    for (const auto& entry : kColorSpaceTable) {
        if (entry.avSpace == space) {
            return entry.omSpace;
        }
    }
    return ColorSpace::Unspecified;
}

/// @brief Maps AVColorTransferCharacteristic to OpenMedia ColorTransfer.
ColorTransfer mapColorTransfer(AVColorTransferCharacteristic trc) noexcept {
    for (const auto& entry : kColorTransferTable) {
        if (entry.avTrc == trc) {
            return entry.omTrc;
        }
    }
    return ColorTransfer::Unspecified;
}

/// @brief Copies extradata from StreamInfo into AVCodecContext with required padding.
CoreStatus copyExtradata(AVCodecContext& ctx, const std::vector<uint8_t>& extradata) noexcept {
    if (extradata.empty()) {
        return CoreStatus::Ok;
    }
    constexpr size_t kMaxIntSize = static_cast<size_t>(std::numeric_limits<int>::max());
    if (extradata.size() > kMaxIntSize - AV_INPUT_BUFFER_PADDING_SIZE) {
        return CoreStatus::InvalidArgument;
    }
    const size_t allocSize = extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE;
    uint8_t* buf = static_cast<uint8_t*>(av_mallocz(allocSize));
    if (buf == nullptr) {
        return CoreStatus::OpenFailed;
    }
    std::memcpy(buf, extradata.data(), extradata.size());
    ctx.extradata = buf;
    ctx.extradata_size = static_cast<int>(extradata.size());
    return CoreStatus::Ok;
}

/// @brief Attempts to attach a hardware device context to AVCodecContext; returns active backend.
HwDecodeBackend tryAttachHardwareDevice(AVCodecContext& ctx,
                                        HwDecodeBackend preferred,
                                        AVBufferRefPtr& outHwCtx) noexcept {
    if (preferred == HwDecodeBackend::None) {
        return HwDecodeBackend::None;
    }
    const HwBackendEntry* entry = findHwBackendEntry(preferred);
    if (entry == nullptr) {
        return HwDecodeBackend::None;
    }
    AVBufferRef* rawHw = nullptr;
    if (av_hwdevice_ctx_create(&rawHw, entry->avType, nullptr, nullptr, 0) >= 0 && rawHw != nullptr) {
        outHwCtx.reset(rawHw);
        ctx.hw_device_ctx = av_buffer_ref(rawHw);
    }
    return preferred;
}

/// @brief Allocates and opens an AVCodecContext for the requested StreamInfo and backend.
CoreStatus openCodecContext(const StreamInfo& stream,
                            HwDecodeBackend preferredBackend,
                            AVCodecContextPtr& outCtx,
                            AVBufferRefPtr& outHwCtx,
                            HwDecodeBackend& outActiveBackend) noexcept {
    const CodecMapEntry* codecEntry = findCodecEntry(stream.codec);
    if (codecEntry == nullptr) {
        return CoreStatus::UnsupportedCodec;
    }

    const AVCodec* codec = findFFmpegDecoder(*codecEntry);
    if (codec == nullptr) {
        return CoreStatus::UnsupportedCodec;
    }

    AVCodecContextPtr ctx(avcodec_alloc_context3(codec));
    if (!ctx) {
        return CoreStatus::OpenFailed;
    }

    ctx->width = stream.width;
    ctx->height = stream.height;
    ctx->thread_count = 0;

    if (copyExtradata(*ctx, stream.extradata) != CoreStatus::Ok) {
        return CoreStatus::OpenFailed;
    }

    AVBufferRefPtr hwCtx;
    HwDecodeBackend activeHw = tryAttachHardwareDevice(*ctx, preferredBackend, hwCtx);
    if (avcodec_open2(ctx.get(), codec, nullptr) < 0) {
        return CoreStatus::OpenFailed;
    }

    outCtx = std::move(ctx);
    outHwCtx = std::move(hwCtx);
    outActiveBackend = activeHw;
    return CoreStatus::Ok;
}

/// @brief Transfers a hardware-backed AVFrame into system memory if hw_frames_ctx is present.
const AVFrame* resolveCpuFrame(const AVFrame& src, AVFramePtr& swScratch) noexcept {
    if (src.hw_frames_ctx == nullptr) {
        return &src;
    }
    if (!swScratch) {
        swScratch.reset(av_frame_alloc());
    }
    if (!swScratch) {
        return nullptr;
    }
    av_frame_unref(swScratch.get());
    if (av_hwframe_transfer_data(swScratch.get(), &src, 0) < 0) {
        return nullptr;
    }
    swScratch->pts = src.pts;
    swScratch->duration = src.duration;
    swScratch->flags = src.flags;
    swScratch->color_range = src.color_range;
    swScratch->colorspace = src.colorspace;
    swScratch->color_trc = src.color_trc;
    return swScratch.get();
}

/// @brief Computes unaligned row byte width and height for a single plane.
void computePlaneDimensions(int32_t width,
                            int32_t height,
                            uint32_t planeIdx,
                            const PixelFormatMapEntry& fmt,
                            uint32_t& outWidthBytes,
                            int32_t& outPlaneHeight) noexcept {
    if (planeIdx == 0U) {
        outWidthBytes = static_cast<uint32_t>(width) * fmt.bytesPerSample;
        outPlaneHeight = height;
        return;
    }
    const int32_t shiftX = static_cast<int32_t>(fmt.chromaShiftX);
    const int32_t shiftY = static_cast<int32_t>(fmt.chromaShiftY);
    const int32_t chromaW = (width + (1 << shiftX) - 1) >> shiftX;
    const uint32_t components = fmt.semiPlanar ? 2U : 1U;
    outWidthBytes = static_cast<uint32_t>(chromaW) * components * fmt.bytesPerSample;
    outPlaneHeight = (height + (1 << shiftY) - 1) >> shiftY;
}

/// @brief Copies a single plane row-by-row into a 16-byte aligned destination buffer.
void copySinglePlane(const uint8_t* srcData,
                     int32_t srcLinesize,
                     std::vector<uint8_t>& dstBuffer,
                     uint32_t dstStride,
                     uint32_t copyWidthBytes,
                     int32_t planeHeight) {
    if (planeHeight <= 0) {
        dstBuffer.clear();
        return;
    }
    dstBuffer.assign(static_cast<size_t>(dstStride) * static_cast<size_t>(planeHeight), 0U);
    if (srcData == nullptr) {
        return;
    }
    const uint32_t srcAbsStride = static_cast<uint32_t>(std::abs(srcLinesize));
    const uint32_t rowCopyBytes = std::min(copyWidthBytes, srcAbsStride);
    for (int32_t row = 0; row < planeHeight; ++row) {
        const uint8_t* srcRow = srcData + (static_cast<ptrdiff_t>(row) * srcLinesize);
        uint8_t* dstRow = dstBuffer.data() + (static_cast<size_t>(row) * dstStride);
        std::memcpy(dstRow, srcRow, rowCopyBytes);
    }
}

/// @brief Converts a decoded AVFrame into a 16-byte row-aligned RawVideoFrame.
CoreStatus convertAVFrameToRaw(const AVFrame& src, RawVideoFrame& dst) {
    const PixelFormatMapEntry* fmt = findPixelFormatEntry(static_cast<AVPixelFormat>(src.format));
    if (fmt == nullptr || src.width <= 0 || src.height <= 0) {
        return CoreStatus::UnsupportedCodec;
    }

    dst.width = src.width;
    dst.height = src.height;
    dst.bitDepth = fmt->bitDepth;
    dst.format = fmt->omFormat;
    dst.colorRange = mapColorRange(src.color_range);
    dst.colorSpace = mapColorSpace(src.colorspace);
    dst.colorTransfer = mapColorTransfer(src.color_trc);
    dst.pts = (src.pts == AV_NOPTS_VALUE) ? 0 : src.pts;
    dst.duration = src.duration;
    dst.isKeyFrame = (src.flags & AV_FRAME_FLAG_KEY) != 0;
    dst.rowBytes.fill(0U);

    for (uint32_t i = 0U; i < fmt->numPlanes; ++i) {
        uint32_t widthBytes = 0U;
        int32_t planeHeight = 0;
        computePlaneDimensions(src.width, src.height, i, *fmt, widthBytes, planeHeight);
        const uint32_t alignedStride = RawVideoFrame::alignRowStride16(widthBytes);
        dst.rowBytes[i] = alignedStride;
        copySinglePlane(src.data[i], src.linesize[i], dst.planes[i], alignedStride, widthBytes, planeHeight);
    }
    return CoreStatus::Ok;
}

/// @brief Maps an FFmpeg avcodec_send_packet return code to CoreStatus.
CoreStatus mapSendStatus(int ret) noexcept {
    if (ret == 0) {
        return CoreStatus::Ok;
    }
    if (ret == AVERROR(EAGAIN)) {
        return CoreStatus::NeedMoreData;
    }
    if (ret == AVERROR_EOF) {
        return CoreStatus::EndOfFile;
    }
    if (ret == AVERROR(EINVAL)) {
        return CoreStatus::InvalidArgument;
    }
    return CoreStatus::DecodeFailed;
}

#else

struct HwBackendEntry {
    HwDecodeBackend backend;
    const char* name;
};

constexpr std::array<HwBackendEntry, 3> kHwBackendTable{{
    {HwDecodeBackend::NVDEC, "nvdec"},
    {HwDecodeBackend::D3D11VA, "d3d11va"},
    {HwDecodeBackend::VideoToolbox, "videotoolbox"},
}};

bool isSupportedVideoCodec(CodecId codec) noexcept {
    return codec == CodecId::AV1 || codec == CodecId::VP9;
}

bool isSupportedHwBackend(HwDecodeBackend backend) noexcept {
    for (const auto& entry : kHwBackendTable) {
        if (entry.backend == backend) {
            return true;
        }
    }
    return false;
}

PixelFormat selectFallbackPixelFormat(const StreamInfo& stream, int32_t bitDepth) noexcept {
    if (stream.pixelFormat != PixelFormat::Unknown) {
        return stream.pixelFormat;
    }
    return (bitDepth > 8) ? PixelFormat::YUV420P10LE : PixelFormat::YUV420P;
}

void buildFallbackFrame(const StreamInfo& stream, const EncodedPacket& pkt, RawVideoFrame& outFrame) {
    const int32_t bitDepth = (stream.bitDepth == 10 || stream.bitDepth == 12) ? stream.bitDepth : 8;
    const uint32_t bytesPerSample = (bitDepth > 8) ? 2U : 1U;
    const uint32_t yStride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(stream.width) * bytesPerSample);
    const int32_t uvW = (stream.width + 1) >> 1;
    const int32_t uvH = (stream.height + 1) >> 1;
    const uint32_t uvStride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(uvW) * bytesPerSample);

    outFrame.width = stream.width;
    outFrame.height = stream.height;
    outFrame.bitDepth = bitDepth;
    outFrame.format = selectFallbackPixelFormat(stream, bitDepth);
    outFrame.colorRange = ColorRange::Limited;
    outFrame.colorSpace = ColorSpace::BT709;
    outFrame.colorTransfer = ColorTransfer::BT709;
    outFrame.pts = pkt.pts;
    outFrame.duration = pkt.duration;
    outFrame.isKeyFrame = pkt.isKeyFrame;
    outFrame.rowBytes = {yStride, uvStride, uvStride, 0U};

    const uint8_t fillByte = pkt.data.empty() ? 0U : pkt.data[0];
    outFrame.planes[0].assign(static_cast<size_t>(yStride) * static_cast<size_t>(stream.height), fillByte);
    outFrame.planes[1].assign(static_cast<size_t>(uvStride) * static_cast<size_t>(uvH), 0x80U);
    outFrame.planes[2].assign(static_cast<size_t>(uvStride) * static_cast<size_t>(uvH), 0x80U);
    outFrame.planes[3].clear();
}

#endif

} // namespace

/// @brief Private implementation holding RAII-managed decoder state.
struct CoreVideoDecoder::Impl {
#ifdef OPENMEDIA_HAS_FFMPEG
    AVCodecContextPtr codecCtx;
    AVBufferRefPtr hwDeviceCtx;
    AVFramePtr frame;
    AVFramePtr swFrame;
    AVPacketPtr packet;
#else
    std::deque<EncodedPacket> pendingPackets;
#endif
    StreamInfo streamInfo{};
    HwDecodeBackend hwBackend = HwDecodeBackend::None;
    bool initialized = false;
    bool flushed = false;
    bool eofReached = false;
    bool hasFirstTimestamp = false;
    int64_t firstTimestampOffset = 0;
    uint64_t decodedFrameCount = 0;
    std::string diagnosticMessage;

    bool isReady() const noexcept {
#ifdef OPENMEDIA_HAS_FFMPEG
        return initialized && codecCtx != nullptr;
#else
        return initialized;
#endif
    }

    void resetCommonState() noexcept {
        flushed = false;
        eofReached = false;
        decodedFrameCount = 0;
        diagnosticMessage.clear();
    }

    void normalizeFrameTimestamp(RawVideoFrame& outFrame) noexcept {
        if (!hasFirstTimestamp) {
            hasFirstTimestamp = true;
            firstTimestampOffset = (outFrame.pts > 0) ? outFrame.pts : 0;
        }
        if (firstTimestampOffset <= 0) {
            return;
        }
        outFrame.pts = (outFrame.pts >= firstTimestampOffset)
                           ? (outFrame.pts - firstTimestampOffset)
                           : 0;
    }

    CoreStatus handleEndOfFile() {
        if (decodedFrameCount == 0U) {
            diagnosticMessage = kNoDecodableFramesMsg;
            return CoreStatus::NoDecodableFrames;
        }
        return CoreStatus::EndOfFile;
    }

    void drainPendingFrames(CoreVideoDecoder& decoder, std::vector<RawVideoFrame>& outFrames) {
        if (decoder.flush() == CoreStatus::DecodeFailed) {
            return;
        }
        RawVideoFrame drained;
        while (decoder.decodeNextFrame(drained) == CoreStatus::Ok) {
            outFrames.push_back(std::move(drained));
            drained = RawVideoFrame{};
        }
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    CoreStatus initBackend(const StreamInfo& stream, HwDecodeBackend preferred) {
        AVCodecContextPtr ctx;
        AVBufferRefPtr hwCtx;
        HwDecodeBackend activeHw = HwDecodeBackend::None;
        const CoreStatus openStatus = openCodecContext(stream, preferred, ctx, hwCtx, activeHw);
        if (openStatus != CoreStatus::Ok) {
            return openStatus;
        }

        AVFramePtr newFrame(av_frame_alloc());
        AVPacketPtr newPkt(av_packet_alloc());
        if (!newFrame || !newPkt) {
            return CoreStatus::OpenFailed;
        }

        codecCtx = std::move(ctx);
        hwDeviceCtx = std::move(hwCtx);
        frame = std::move(newFrame);
        packet = std::move(newPkt);
        streamInfo = stream;
        hwBackend = activeHw;
        initialized = true;
        resetCommonState();
        return CoreStatus::Ok;
    }

    CoreStatus fallbackToCpu() {
        const uint64_t savedCount = decodedFrameCount;
        const bool savedHasFirst = hasFirstTimestamp;
        const int64_t savedOffset = firstTimestampOffset;

        hwDeviceCtx.reset();
        codecCtx.reset();
        const CoreStatus status = initBackend(streamInfo, HwDecodeBackend::None);
        decodedFrameCount = savedCount;
        hasFirstTimestamp = savedHasFirst;
        firstTimestampOffset = savedOffset;
        return status;
    }

    CoreStatus sendPacketBackend(const EncodedPacket& inPkt) {
        av_packet_unref(packet.get());
        if (av_new_packet(packet.get(), static_cast<int>(inPkt.data.size())) < 0) {
            return CoreStatus::DecodeFailed;
        }
        std::memcpy(packet->data, inPkt.data.data(), inPkt.data.size());
        packet->pts = inPkt.pts;
        packet->dts = inPkt.dts;
        packet->duration = inPkt.duration;
        packet->flags = inPkt.isKeyFrame ? AV_PKT_FLAG_KEY : 0;

        const int ret = avcodec_send_packet(codecCtx.get(), packet.get());
        av_packet_unref(packet.get());
        return mapSendStatus(ret);
    }

    int receiveWithFlushRetry() noexcept {
        int ret = avcodec_receive_frame(codecCtx.get(), frame.get());
        if (ret == AVERROR(EAGAIN) && flushed) {
            avcodec_send_packet(codecCtx.get(), nullptr);
            ret = avcodec_receive_frame(codecCtx.get(), frame.get());
        }
        return ret;
    }

    CoreStatus decodeFrameBackend(RawVideoFrame& outFrame) {
        av_frame_unref(frame.get());
        const int ret = receiveWithFlushRetry();
        if (ret == AVERROR(EAGAIN)) {
            return CoreStatus::NeedMoreData;
        }
        if (ret == AVERROR_EOF) {
            eofReached = true;
            return CoreStatus::EndOfFile;
        }
        if (ret < 0) {
            return CoreStatus::DecodeFailed;
        }
        const AVFrame* cpuFrame = resolveCpuFrame(*frame, swFrame);
        if (cpuFrame == nullptr) {
            av_frame_unref(frame.get());
            return CoreStatus::DecodeFailed;
        }
        const CoreStatus status = convertAVFrameToRaw(*cpuFrame, outFrame);
        av_frame_unref(frame.get());
        return status;
    }
#else
    CoreStatus initBackend(const StreamInfo& stream, HwDecodeBackend preferred) {
        if (!isSupportedVideoCodec(stream.codec)) {
            return CoreStatus::UnsupportedCodec;
        }
        streamInfo = stream;
        hwBackend = isSupportedHwBackend(preferred) ? preferred : HwDecodeBackend::None;
        pendingPackets.clear();
        initialized = true;
        resetCommonState();
        return CoreStatus::Ok;
    }

    CoreStatus fallbackToCpu() {
        const uint64_t savedCount = decodedFrameCount;
        const bool savedHasFirst = hasFirstTimestamp;
        const int64_t savedOffset = firstTimestampOffset;

        pendingPackets.clear();
        hwBackend = HwDecodeBackend::None;
        resetCommonState();
        decodedFrameCount = savedCount;
        hasFirstTimestamp = savedHasFirst;
        firstTimestampOffset = savedOffset;
        return CoreStatus::Ok;
    }

    CoreStatus sendPacketBackend(const EncodedPacket& inPkt) {
        pendingPackets.push_back(inPkt);
        return CoreStatus::Ok;
    }

    CoreStatus decodeFrameBackend(RawVideoFrame& outFrame) {
        if (!pendingPackets.empty()) {
            const EncodedPacket pkt = std::move(pendingPackets.front());
            pendingPackets.pop_front();
            buildFallbackFrame(streamInfo, pkt, outFrame);
            return CoreStatus::Ok;
        }
        return eofReached ? CoreStatus::EndOfFile : CoreStatus::NeedMoreData;
    }
#endif
};

CoreVideoDecoder::CoreVideoDecoder()
    : impl_(std::make_unique<Impl>()) {}

CoreVideoDecoder::~CoreVideoDecoder() = default;

CoreVideoDecoder::CoreVideoDecoder(CoreVideoDecoder&& other) noexcept = default;

CoreVideoDecoder& CoreVideoDecoder::operator=(CoreVideoDecoder&& other) noexcept = default;

CoreStatus CoreVideoDecoder::initialize(const StreamInfo& stream) {
    return initializeWithHardware(stream, HwDecodeBackend::None);
}

CoreStatus CoreVideoDecoder::initializeWithHardware(const StreamInfo& stream,
                                                    HwDecodeBackend preferredBackend) {
    if (!impl_) {
        return CoreStatus::NotInitialized;
    }
    if (stream.type != MediaType::Video || stream.width <= 0 || stream.height <= 0) {
        return CoreStatus::InvalidArgument;
    }
    impl_->hasFirstTimestamp = false;
    impl_->firstTimestampOffset = 0;
    return impl_->initBackend(stream, preferredBackend);
}

CoreStatus CoreVideoDecoder::sendPacket(const EncodedPacket& packet) {
    if (!impl_ || !impl_->isReady()) {
        return CoreStatus::NotInitialized;
    }
    constexpr size_t kMaxIntSize = static_cast<size_t>(std::numeric_limits<int>::max());
    if (packet.data.empty() || packet.data.size() > kMaxIntSize) {
        return CoreStatus::InvalidArgument;
    }
    impl_->flushed = false;
    impl_->eofReached = false;
    return impl_->sendPacketBackend(packet);
}

CoreStatus CoreVideoDecoder::decodeNextFrame(RawVideoFrame& outFrame) {
    if (!impl_ || !impl_->isReady()) {
        return CoreStatus::NotInitialized;
    }
    const CoreStatus status = impl_->decodeFrameBackend(outFrame);
    if (status == CoreStatus::Ok) {
        impl_->normalizeFrameTimestamp(outFrame);
        ++impl_->decodedFrameCount;
        return CoreStatus::Ok;
    }
    if (status == CoreStatus::EndOfFile) {
        return impl_->handleEndOfFile();
    }
    return status;
}

CoreStatus CoreVideoDecoder::flush() {
    if (!impl_ || !impl_->isReady()) {
        return CoreStatus::NotInitialized;
    }
    impl_->flushed = true;
#ifdef OPENMEDIA_HAS_FFMPEG
    const int ret = avcodec_send_packet(impl_->codecCtx.get(), nullptr);
    if (ret == 0 || ret == AVERROR_EOF) {
        return CoreStatus::Ok;
    }
    return CoreStatus::DecodeFailed;
#else
    impl_->eofReached = true;
    return CoreStatus::Ok;
#endif
}

void CoreVideoDecoder::reset() {
    if (!impl_ || !impl_->isReady()) {
        return;
    }
    impl_->resetCommonState();
#ifdef OPENMEDIA_HAS_FFMPEG
    avcodec_flush_buffers(impl_->codecCtx.get());
#else
    impl_->pendingPackets.clear();
#endif
}

void CoreVideoDecoder::resetFirstTimestamp(int64_t startTimePts) noexcept {
    if (!impl_) {
        return;
    }
    if (startTimePts > 0) {
        impl_->hasFirstTimestamp = true;
        impl_->firstTimestampOffset = startTimePts;
        return;
    }
    impl_->hasFirstTimestamp = false;
    impl_->firstTimestampOffset = 0;
}

CoreStatus CoreVideoDecoder::drainBeforeFallback(std::vector<RawVideoFrame>& drainedFrames) {
    if (!impl_ || !impl_->isReady()) {
        return CoreStatus::NotInitialized;
    }
    drainedFrames.clear();
    if (impl_->hwBackend == HwDecodeBackend::None) {
        return CoreStatus::Ok;
    }
    impl_->drainPendingFrames(*this, drainedFrames);
    return impl_->fallbackToCpu();
}

HwDecodeBackend CoreVideoDecoder::activeBackend() const noexcept {
    if (!impl_) {
        return HwDecodeBackend::None;
    }
    return impl_->hwBackend;
}

bool CoreVideoDecoder::isHardwareActive() const noexcept {
    return activeBackend() != HwDecodeBackend::None;
}

const std::string& CoreVideoDecoder::lastDiagnosticMessage() const noexcept {
    static const std::string kEmptyMsg;
    if (!impl_) {
        return kEmptyMsg;
    }
    return impl_->diagnosticMessage;
}


namespace {

std::mutex& telemetryMutex() {
    static std::mutex mtx;
    return mtx;
}

std::vector<OpenMediaFrameDecodeTelemetry>& telemetryStore() {
    static std::vector<OpenMediaFrameDecodeTelemetry> store;
    return store;
}

std::string escapeTelemetryJson(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (ch == '"' || ch == '\\') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

} // namespace

void publishDecodeTelemetry(const OpenMediaFrameDecodeTelemetry& entry) {
    std::lock_guard<std::mutex> lock(telemetryMutex());
    auto& store = telemetryStore();
    for (auto& existing : store) {
        if (existing.fileId == entry.fileId && existing.filePath == entry.filePath) {
            existing = entry;
            return;
        }
    }
    store.push_back(entry);
}

std::vector<OpenMediaFrameDecodeTelemetry> getActiveDecodeTelemetrySnapshot() {
    std::lock_guard<std::mutex> lock(telemetryMutex());
    return telemetryStore();
}

void clearDecodeTelemetry() {
    std::lock_guard<std::mutex> lock(telemetryMutex());
    telemetryStore().clear();
}

bool writeTelemetryJsonSnapshot(const std::string& jsonPath) {
    if (jsonPath.empty()) {
        return false;
    }
    const auto snapshot = getActiveDecodeTelemetrySnapshot();
    std::ofstream ofs(jsonPath, std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << "{\n  \"streams\": [\n";
    for (size_t i = 0; i < snapshot.size(); ++i) {
        const auto& e = snapshot[i];
        ofs << "    {\n"
            << "      \"fileId\": " << e.fileId << ",\n"
            << "      \"filePath\": \"" << escapeTelemetryJson(e.filePath) << "\",\n"
            << "      \"hostAppName\": \"" << escapeTelemetryJson(e.hostAppName) << "\",\n"
            << "      \"codecName\": \"" << escapeTelemetryJson(e.codecName) << "\",\n"
            << "      \"width\": " << e.width << ",\n"
            << "      \"height\": " << e.height << ",\n"
            << "      \"pixelFormat\": \"" << escapeTelemetryJson(e.pixelFormat) << "\",\n"
            << "      \"colorSpace\": \"" << escapeTelemetryJson(e.colorSpace) << "\",\n"
            << "      \"decodedOnGpu\": " << (e.decodedOnGpu ? "true" : "false") << ",\n"
            << "      \"activeBackend\": \"" << escapeTelemetryJson(e.activeBackend) << "\",\n"
            << "      \"policyReason\": \"" << escapeTelemetryJson(e.policyReason) << "\"\n"
            << "    }" << (i + 1 < snapshot.size() ? "," : "") << "\n";
    }
    ofs << "  ]\n}\n";
    return ofs.good();
}

} // namespace openmedia
