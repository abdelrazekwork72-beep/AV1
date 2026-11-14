/// @file video_encoder.cpp
/// @brief CoreVideoEncoder implementation using FFmpeg libavcodec with RAII deleters and lookup tables.

#include "openmedia/video_encoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
}

#include <algorithm>
#include <cstring>
#include <mutex>
#include <utility>

namespace openmedia {
namespace {

struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const noexcept {
        avcodec_free_context(&ctx);
    }
};

struct AVFrameDeleter {
    void operator()(AVFrame* frame) const noexcept {
        av_frame_free(&frame);
    }
};

struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const noexcept {
        av_packet_free(&pkt);
    }
};

using AVCodecCtxPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct PixelFormatMapping {
    PixelFormat coreFmt;
    AVPixelFormat avFmt;
    int32_t planes;
    int32_t chromaShiftY;
    int32_t bytesPerSample;
};

constexpr PixelFormatMapping kPixelFormatTable[] = {
    {PixelFormat::YUV420P,     AV_PIX_FMT_YUV420P,     3, 1, 1},
    {PixelFormat::YUV422P,     AV_PIX_FMT_YUV422P,     3, 0, 1},
    {PixelFormat::YUV444P,     AV_PIX_FMT_YUV444P,     3, 0, 1},
    {PixelFormat::YUV420P10LE, AV_PIX_FMT_YUV420P10LE, 3, 1, 2},
    {PixelFormat::YUV422P10LE, AV_PIX_FMT_YUV422P10LE, 3, 0, 2},
    {PixelFormat::YUV444P10LE, AV_PIX_FMT_YUV444P10LE, 3, 0, 2},
    {PixelFormat::YUV420P12LE, AV_PIX_FMT_YUV420P12LE, 3, 1, 2},
    {PixelFormat::YUV422P12LE, AV_PIX_FMT_YUV422P12LE, 3, 0, 2},
    {PixelFormat::YUV444P12LE, AV_PIX_FMT_YUV444P12LE, 3, 0, 2},
    {PixelFormat::NV12,        AV_PIX_FMT_NV12,        2, 1, 1},
    {PixelFormat::P010LE,      AV_PIX_FMT_P010LE,      2, 1, 2},
    {PixelFormat::BGRA8,       AV_PIX_FMT_BGRA,        1, 0, 4},
    {PixelFormat::RGBA8,       AV_PIX_FMT_RGBA,        1, 0, 4},
};

struct ColorRangeMapping {
    ColorRange coreRange;
    AVColorRange avRange;
};

constexpr ColorRangeMapping kColorRangeTable[] = {
    {ColorRange::Unspecified, AVCOL_RANGE_UNSPECIFIED},
    {ColorRange::Limited,     AVCOL_RANGE_MPEG},
    {ColorRange::Full,        AVCOL_RANGE_JPEG},
};

struct ColorSpaceMapping {
    ColorSpace coreSpace;
    AVColorSpace avSpace;
};

constexpr ColorSpaceMapping kColorSpaceTable[] = {
    {ColorSpace::Unspecified, AVCOL_SPC_UNSPECIFIED},
    {ColorSpace::BT709,       AVCOL_SPC_BT709},
    {ColorSpace::BT2020_NCL,  AVCOL_SPC_BT2020_NCL},
    {ColorSpace::BT2020_CL,   AVCOL_SPC_BT2020_CL},
    {ColorSpace::BT601,       AVCOL_SPC_SMPTE170M},
};

struct ColorTransferMapping {
    ColorTransfer coreTrc;
    AVColorTransferCharacteristic avTrc;
};

constexpr ColorTransferMapping kColorTransferTable[] = {
    {ColorTransfer::Unspecified, AVCOL_TRC_UNSPECIFIED},
    {ColorTransfer::BT709,       AVCOL_TRC_BT709},
    {ColorTransfer::PQ_ST2084,   AVCOL_TRC_SMPTE2084},
    {ColorTransfer::HLG,         AVCOL_TRC_ARIB_STD_B67},
    {ColorTransfer::sRGB,        AVCOL_TRC_IEC61966_2_1},
};

const PixelFormatMapping* findPixelFormatMapping(PixelFormat fmt) noexcept {
    for (const auto& entry : kPixelFormatTable) {
        if (entry.coreFmt == fmt) {
            return &entry;
        }
    }
    return nullptr;
}

AVColorRange lookupColorRange(ColorRange range) noexcept {
    for (const auto& entry : kColorRangeTable) {
        if (entry.coreRange == range) {
            return entry.avRange;
        }
    }
    return AVCOL_RANGE_UNSPECIFIED;
}

AVColorSpace lookupColorSpace(ColorSpace space) noexcept {
    for (const auto& entry : kColorSpaceTable) {
        if (entry.coreSpace == space) {
            return entry.avSpace;
        }
    }
    return AVCOL_SPC_UNSPECIFIED;
}

AVColorTransferCharacteristic lookupColorTransfer(ColorTransfer trc) noexcept {
    for (const auto& entry : kColorTransferTable) {
        if (entry.coreTrc == trc) {
            return entry.avTrc;
        }
    }
    return AVCOL_TRC_UNSPECIFIED;
}

const AVCodec* selectEncoder(CodecId codec, bool preferHardware, const char*& outName) noexcept {
    if (codec == CodecId::VP9) {
        outName = "libvpx-vp9";
        return avcodec_find_encoder_by_name(outName);
    }
    if (codec != CodecId::AV1) {
        outName = nullptr;
        return nullptr;
    }
    if (preferHardware) {
        if (const AVCodec* nvenc = avcodec_find_encoder_by_name("av1_nvenc")) {
            outName = "av1_nvenc";
            return nvenc;
        }
    }
    outName = "libsvtav1";
    return avcodec_find_encoder_by_name(outName);
}

bool isSupportedVideoCodec(CodecId codec) noexcept {
    return codec == CodecId::AV1 || codec == CodecId::VP9;
}

bool isSupportedBitDepth(int32_t bitDepth) noexcept {
    return bitDepth == 8 || bitDepth == 10 || bitDepth == 12;
}

bool hasValidTimingAndBounds(const VideoEncodeConfig& config) noexcept {
    if (config.width <= 0 || config.height <= 0) {
        return false;
    }
    if (config.gopSize < 0 || config.bitrateBps < 0) {
        return false;
    }
    return config.frameRate.num > 0;
}

bool hasValidFrameSpec(const VideoEncodeConfig& config) noexcept {
    return isSupportedBitDepth(config.bitDepth) &&
           findPixelFormatMapping(config.pixelFormat) != nullptr;
}

CoreStatus validateConfig(const VideoEncodeConfig& config) noexcept {
    if (!isSupportedVideoCodec(config.codec)) {
        return CoreStatus::UnsupportedCodec;
    }
    if (!hasValidTimingAndBounds(config) || !hasValidFrameSpec(config)) {
        return CoreStatus::InvalidArgument;
    }
    return CoreStatus::Ok;
}

void configureRateControl(AVCodecContext* ctx, const VideoEncodeConfig& config, const char* encoderName) noexcept {
    if (config.bitrateBps > 0) {
        ctx->bit_rate = config.bitrateBps;
        return;
    }
    const int32_t clampedCrf = std::clamp(config.crf, 0, 63);
    if (std::strcmp(encoderName, "av1_nvenc") == 0) {
        av_opt_set(ctx->priv_data, "rc", "vbr", 0);
        av_opt_set_int(ctx->priv_data, "cq", CoreVideoEncoder::normalizeCrfToVp9Cq(clampedCrf), 0);
        return;
    }
    if (std::strcmp(encoderName, "libvpx-vp9") == 0) {
        ctx->bit_rate = 0;
        av_opt_set_int(ctx->priv_data, "crf", clampedCrf, 0);
        av_opt_set_int(ctx->priv_data, "cpu-used", std::clamp(config.preset, 0, 8), 0);
        return;
    }
    av_opt_set_int(ctx->priv_data, "crf", clampedCrf, 0);
    av_opt_set_int(ctx->priv_data, "preset", std::clamp(config.preset, 0, 13), 0);
}

void copyPlaneRows(const uint8_t* src, uint32_t srcStride, uint8_t* dst, int dstStride, int rows) noexcept {
    const uint32_t copyBytes = std::min(srcStride, static_cast<uint32_t>(dstStride));
    for (int r = 0; r < rows; ++r) {
        std::memcpy(dst + static_cast<size_t>(r) * dstStride,
                    src + static_cast<size_t>(r) * srcStride,
                    copyBytes);
    }
}

bool isValidSourcePlane(const RawVideoFrame& src, int32_t planeIdx,
                        const AVFrame* dst, int rows) noexcept {
    if (src.planes[planeIdx].empty() || src.rowBytes[planeIdx] == 0) {
        return false;
    }
    if (dst->data[planeIdx] == nullptr) {
        return false;
    }
    const size_t requiredBytes = static_cast<size_t>(rows) * src.rowBytes[planeIdx];
    return src.planes[planeIdx].size() >= requiredBytes;
}

CoreStatus copyFramePlanes(const RawVideoFrame& src, const PixelFormatMapping& map, AVFrame* dst) noexcept {
    for (int32_t p = 0; p < map.planes; ++p) {
        const int rows = (p == 0) ? src.height : (src.height >> map.chromaShiftY);
        if (!isValidSourcePlane(src, p, dst, rows)) {
            return CoreStatus::InvalidArgument;
        }
        copyPlaneRows(src.planes[p].data(), src.rowBytes[p], dst->data[p], dst->linesize[p], rows);
    }
    return CoreStatus::Ok;
}

std::vector<uint8_t> copyCodecExtradata(const AVCodecContext* ctx) {
    if (ctx == nullptr || ctx->extradata == nullptr || ctx->extradata_size <= 0) {
        return {};
    }
    return std::vector<uint8_t>(ctx->extradata, ctx->extradata + ctx->extradata_size);
}

bool matchesEncoderConfig(const RawVideoFrame& frame, const VideoEncodeConfig& config) noexcept {
    return frame.width == config.width &&
           frame.height == config.height &&
           frame.format == config.pixelFormat;
}

CoreStatus prepareAVFrame(const RawVideoFrame& frame, AVFramePtr& outFrame) {
    const PixelFormatMapping* pixMap = findPixelFormatMapping(frame.format);
    AVFramePtr avFrame(av_frame_alloc());
    if (pixMap == nullptr || !avFrame) {
        return CoreStatus::EncodeFailed;
    }
    avFrame->width = frame.width;
    avFrame->height = frame.height;
    avFrame->format = pixMap->avFmt;
    avFrame->pts = frame.pts;
    avFrame->duration = frame.duration;
    avFrame->color_range = lookupColorRange(frame.colorRange);
    avFrame->colorspace = lookupColorSpace(frame.colorSpace);
    avFrame->color_trc = lookupColorTransfer(frame.colorTransfer);

    if (av_frame_get_buffer(avFrame.get(), 16) < 0 || av_frame_make_writable(avFrame.get()) < 0) {
        return CoreStatus::EncodeFailed;
    }
    const CoreStatus copyStatus = copyFramePlanes(frame, *pixMap, avFrame.get());
    if (copyStatus != CoreStatus::Ok) {
        return copyStatus;
    }
    outFrame = std::move(avFrame);
    return CoreStatus::Ok;
}

CoreStatus drainPackets(AVCodecContext* ctx, std::vector<EncodedPacket>& outPackets) {
    AVPacketPtr pkt(av_packet_alloc());
    if (!pkt) {
        return CoreStatus::EncodeFailed;
    }
    while (true) {
        const int ret = avcodec_receive_packet(ctx, pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return CoreStatus::Ok;
        }
        if (ret < 0) {
            return CoreStatus::EncodeFailed;
        }
        EncodedPacket outPkt;
        outPkt.streamIndex = pkt->stream_index;
        outPkt.pts = pkt->pts;
        outPkt.dts = pkt->dts;
        outPkt.duration = pkt->duration;
        outPkt.isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        outPkt.data.assign(pkt->data, pkt->data + pkt->size);
        outPackets.push_back(std::move(outPkt));
        av_packet_unref(pkt.get());
    }
}

} // namespace

struct CoreVideoEncoder::Impl {
    std::mutex mutex;
    VideoEncodeConfig config{};
    AVCodecCtxPtr codecCtx;
    std::vector<uint8_t> extradata;
    bool flushed = false;
};

CoreVideoEncoder::CoreVideoEncoder() : impl_(std::make_unique<Impl>()) {}

CoreVideoEncoder::~CoreVideoEncoder() = default;

CoreVideoEncoder::CoreVideoEncoder(CoreVideoEncoder&&) noexcept = default;

CoreVideoEncoder& CoreVideoEncoder::operator=(CoreVideoEncoder&&) noexcept = default;

CoreStatus CoreVideoEncoder::initialize(const VideoEncodeConfig& config) {
    const CoreStatus status = validateConfig(config);
    if (status != CoreStatus::Ok) {
        return status;
    }

    const char* encoderName = nullptr;
    const AVCodec* codec = selectEncoder(config.codec, config.preferHardware, encoderName);
    if (codec == nullptr) {
        return CoreStatus::UnsupportedCodec;
    }

    AVCodecCtxPtr ctx(avcodec_alloc_context3(codec));
    if (!ctx) {
        return CoreStatus::OpenFailed;
    }

    const PixelFormatMapping* pixMap = findPixelFormatMapping(config.pixelFormat);
    const RationalTime safeTimeBase = RationalTime::sanitize(config.timeBase.num, config.timeBase.den);
    const RationalTime safeFrameRate = RationalTime::sanitize(config.frameRate.num, config.frameRate.den);

    ctx->width = config.width;
    ctx->height = config.height;
    ctx->pix_fmt = pixMap->avFmt;
    ctx->time_base = AVRational{static_cast<int>(safeTimeBase.num), static_cast<int>(safeTimeBase.den)};
    ctx->framerate = AVRational{static_cast<int>(safeFrameRate.num), static_cast<int>(safeFrameRate.den)};
    ctx->gop_size = config.gopSize;
    ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    configureRateControl(ctx.get(), config, encoderName);

    if (avcodec_open2(ctx.get(), codec, nullptr) < 0) {
        return CoreStatus::OpenFailed;
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->config = config;
    impl_->flushed = false;
    impl_->extradata = copyCodecExtradata(ctx.get());
    impl_->codecCtx = std::move(ctx);
    return CoreStatus::Ok;
}

CoreStatus CoreVideoEncoder::encodeFrame(const RawVideoFrame& frame, std::vector<EncodedPacket>& outPackets) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->codecCtx) {
        return CoreStatus::NotInitialized;
    }
    if (!matchesEncoderConfig(frame, impl_->config)) {
        return CoreStatus::InvalidArgument;
    }

    AVFramePtr avFrame;
    const CoreStatus prepStatus = prepareAVFrame(frame, avFrame);
    if (prepStatus != CoreStatus::Ok) {
        return prepStatus;
    }

    if (avcodec_send_frame(impl_->codecCtx.get(), avFrame.get()) < 0) {
        return CoreStatus::EncodeFailed;
    }
    return drainPackets(impl_->codecCtx.get(), outPackets);
}

CoreStatus CoreVideoEncoder::flush(std::vector<EncodedPacket>& outPackets) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->codecCtx) {
        return CoreStatus::NotInitialized;
    }
    if (impl_->flushed) {
        return CoreStatus::Ok;
    }
    const int ret = avcodec_send_frame(impl_->codecCtx.get(), nullptr);
    if (ret < 0 && ret != AVERROR_EOF) {
        return CoreStatus::EncodeFailed;
    }
    impl_->flushed = true;
    return drainPackets(impl_->codecCtx.get(), outPackets);
}

std::vector<uint8_t> CoreVideoEncoder::getExtradata() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->extradata;
}

} // namespace openmedia
