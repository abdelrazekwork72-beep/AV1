/// @file video_decoder.cpp
/// @brief Implementation of CoreVideoDecoder using FFmpeg libavcodec (libdav1d / libvpx-vp9).

#include "openmedia/video_decoder.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>
}

namespace openmedia {

namespace {

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

using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

/// @brief Static mapping entry from OpenMedia CodecId to FFmpeg decoder name and AVCodecID.
struct CodecMapEntry {
    CodecId codecId;
    const char* decoderName;
    AVCodecID avCodecId;
};

/// @brief Supported software video decoders (AV1 via libdav1d, VP9 via libvpx-vp9).
constexpr std::array<CodecMapEntry, 2> kCodecTable{{
    {CodecId::AV1, "libdav1d", AV_CODEC_ID_AV1},
    {CodecId::VP9, "libvpx-vp9", AV_CODEC_ID_VP9},
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
    dstBuffer.assign(static_cast<size_t>(dstStride) * static_cast<size_t>(planeHeight), 0U);
    if (srcData == nullptr || planeHeight <= 0) {
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
    dst.pts = src.pts;
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

} // namespace

/// @brief Private implementation holding RAII-managed FFmpeg decoder state.
struct CoreVideoDecoder::Impl {
    AVCodecContextPtr codecCtx;
    AVFramePtr frame;
    AVPacketPtr packet;
    bool initialized = false;
};

CoreVideoDecoder::CoreVideoDecoder()
    : impl_(std::make_unique<Impl>()) {}

CoreVideoDecoder::~CoreVideoDecoder() = default;

CoreVideoDecoder::CoreVideoDecoder(CoreVideoDecoder&& other) noexcept = default;

CoreVideoDecoder& CoreVideoDecoder::operator=(CoreVideoDecoder&& other) noexcept = default;

CoreStatus CoreVideoDecoder::initialize(const StreamInfo& stream) {
    if (stream.type != MediaType::Video) {
        return CoreStatus::InvalidArgument;
    }

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

    if (avcodec_open2(ctx.get(), codec, nullptr) < 0) {
        return CoreStatus::OpenFailed;
    }

    AVFramePtr frame(av_frame_alloc());
    AVPacketPtr pkt(av_packet_alloc());
    if (!frame || !pkt) {
        return CoreStatus::OpenFailed;
    }

    impl_->codecCtx = std::move(ctx);
    impl_->frame = std::move(frame);
    impl_->packet = std::move(pkt);
    impl_->initialized = true;
    return CoreStatus::Ok;
}

CoreStatus CoreVideoDecoder::sendPacket(const EncodedPacket& packet) {
    if (!impl_->initialized || !impl_->codecCtx) {
        return CoreStatus::NotInitialized;
    }
    if (packet.data.empty()) {
        return CoreStatus::InvalidArgument;
    }

    av_packet_unref(impl_->packet.get());
    if (av_new_packet(impl_->packet.get(), static_cast<int>(packet.data.size())) < 0) {
        return CoreStatus::DecodeFailed;
    }

    std::memcpy(impl_->packet->data, packet.data.data(), packet.data.size());
    impl_->packet->pts = packet.pts;
    impl_->packet->dts = packet.dts;
    impl_->packet->duration = packet.duration;
    impl_->packet->flags = packet.isKeyFrame ? AV_PKT_FLAG_KEY : 0;

    const int ret = avcodec_send_packet(impl_->codecCtx.get(), impl_->packet.get());
    av_packet_unref(impl_->packet.get());
    return mapSendStatus(ret);
}

CoreStatus CoreVideoDecoder::decodeNextFrame(RawVideoFrame& outFrame) {
    if (!impl_->initialized || !impl_->codecCtx) {
        return CoreStatus::NotInitialized;
    }

    av_frame_unref(impl_->frame.get());
    const int ret = avcodec_receive_frame(impl_->codecCtx.get(), impl_->frame.get());
    if (ret == AVERROR(EAGAIN)) {
        return CoreStatus::NeedMoreData;
    }
    if (ret == AVERROR_EOF) {
        return CoreStatus::EndOfFile;
    }
    if (ret < 0) {
        return CoreStatus::DecodeFailed;
    }

    const CoreStatus status = convertAVFrameToRaw(*impl_->frame, outFrame);
    av_frame_unref(impl_->frame.get());
    return status;
}

CoreStatus CoreVideoDecoder::flush() {
    if (!impl_->initialized || !impl_->codecCtx) {
        return CoreStatus::NotInitialized;
    }
    const int ret = avcodec_send_packet(impl_->codecCtx.get(), nullptr);
    if (ret == 0 || ret == AVERROR_EOF) {
        return CoreStatus::Ok;
    }
    return CoreStatus::DecodeFailed;
}

void CoreVideoDecoder::reset() {
    if (impl_->initialized && impl_->codecCtx) {
        avcodec_flush_buffers(impl_->codecCtx.get());
    }
}

} // namespace openmedia
