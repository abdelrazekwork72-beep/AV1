/// @file muxer.cpp
/// @brief CoreMuxer and CoreAudioEncoder implementations with FFmpeg RAII deleters and C++17 fallback.

#include "openmedia/muxer.hpp"

#ifdef OPENMEDIA_HAS_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
}
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace openmedia {

namespace {

bool isSupportedContainerFormat(const std::string& formatName) {
    return formatName == "matroska" || formatName == "webm" || formatName == "mp4";
}

bool isValidAudioConfig(const AudioEncodeConfig& config) noexcept {
    return config.sampleRate > 0 && config.channels > 0;
}

bool hasSufficientChannelSamples(const RawAudioBatch& batch) noexcept {
    for (int32_t ch = 0; ch < batch.channels; ++ch) {
        if (static_cast<int32_t>(batch.channelData[ch].size()) < batch.numSamples) {
            return false;
        }
    }
    return true;
}

int16_t floatToS16(float sample) noexcept {
    const float clamped = std::max(-1.0f, std::min(1.0f, sample));
    return static_cast<int16_t>(std::lrintf(clamped * 32767.0f));
}

void consumeFifoSamples(std::vector<std::vector<float>>& fifo, int32_t count) {
    for (auto& chBuf : fifo) {
        chBuf.erase(chBuf.begin(), chBuf.begin() + count);
    }
}

#ifdef OPENMEDIA_HAS_FFMPEG

bool hasOpenAvio(const AVFormatContext* ctx) noexcept {
    return ctx->oformat && !(ctx->oformat->flags & AVFMT_NOFILE) && ctx->pb;
}

/// @brief Custom RAII deleter for output AVFormatContext (closes AVIO if open).
struct AVFormatOutputDeleter {
    void operator()(AVFormatContext* ctx) const noexcept {
        if (!ctx) {
            return;
        }
        if (hasOpenAvio(ctx)) {
            avio_closep(&ctx->pb);
        }
        avformat_free_context(ctx);
    }
};

/// @brief Custom RAII deleter for AVCodecContext.
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const noexcept {
        avcodec_free_context(&ctx);
    }
};

/// @brief Custom RAII deleter for SwsContext per US-1.2.0 Section 2.1.
struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const noexcept {
        if (ctx != nullptr) {
            sws_freeContext(ctx);
        }
    }
};

/// @brief Custom RAII deleter for AVFrame.
struct AVFrameDeleter {
    void operator()(AVFrame* frame) const noexcept {
        av_frame_free(&frame);
    }
};

/// @brief Custom RAII deleter for AVPacket.
struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const noexcept {
        av_packet_free(&pkt);
    }
};

using AVFormatOutputPtr = std::unique_ptr<AVFormatContext, AVFormatOutputDeleter>;
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

AVCodecID mapCodecId(CodecId codec) noexcept {
    static constexpr std::pair<CodecId, AVCodecID> kCodecMap[] = {
        {CodecId::AV1, AV_CODEC_ID_AV1},
        {CodecId::VP9, AV_CODEC_ID_VP9},
        {CodecId::VP8, AV_CODEC_ID_VP8},
        {CodecId::H264, AV_CODEC_ID_H264},
        {CodecId::HEVC, AV_CODEC_ID_HEVC},
        {CodecId::ProRes, AV_CODEC_ID_PRORES},
        {CodecId::FFV1, AV_CODEC_ID_FFV1},
        {CodecId::Opus, AV_CODEC_ID_OPUS},
        {CodecId::FLAC, AV_CODEC_ID_FLAC},
        {CodecId::Vorbis, AV_CODEC_ID_VORBIS},
        {CodecId::PCM, AV_CODEC_ID_PCM_S16LE},
        {CodecId::SRT, AV_CODEC_ID_SUBRIP},
        {CodecId::WebVTT, AV_CODEC_ID_WEBVTT},
        {CodecId::XHE_AAC, AV_CODEC_ID_AAC},
        {CodecId::AAC, AV_CODEC_ID_AAC},
        {CodecId::MP3, AV_CODEC_ID_MP3},
        {CodecId::AC3, AV_CODEC_ID_AC3},
        {CodecId::DTS, AV_CODEC_ID_DTS},
    };
    for (const auto& entry : kCodecMap) {
        if (entry.first == codec) {
            return entry.second;
        }
    }
    return AV_CODEC_ID_NONE;
}

AVMediaType mapMediaType(MediaType type) noexcept {
    static constexpr std::pair<MediaType, AVMediaType> kMediaTypeMap[] = {
        {MediaType::Video, AVMEDIA_TYPE_VIDEO},
        {MediaType::Audio, AVMEDIA_TYPE_AUDIO},
        {MediaType::Subtitle, AVMEDIA_TYPE_SUBTITLE},
    };
    for (const auto& entry : kMediaTypeMap) {
        if (entry.first == type) {
            return entry.second;
        }
    }
    return AVMEDIA_TYPE_UNKNOWN;
}

int64_t sanitizePositiveNum(int64_t value) noexcept {
    if (value > 0) {
        return value;
    }
    return 1;
}

int32_t sanitizeChannels(int32_t channels) noexcept {
    if (channels > 0) {
        return channels;
    }
    return 2;
}

void applyCommonStreamParams(AVStream* stream, AVCodecID avCodec, AVMediaType avType, const StreamInfo& info) {
    stream->codecpar->codec_type = avType;
    stream->codecpar->codec_id = avCodec;
    const int64_t safeNum = sanitizePositiveNum(info.timeBase.num);
    RationalTime tb = RationalTime::sanitize(safeNum, info.timeBase.den);
    stream->time_base = AVRational{static_cast<int>(tb.num), static_cast<int>(tb.den)};
}

void applyMediaSpecificParams(AVStream* stream, AVMediaType avType, const StreamInfo& info) {
    if (avType == AVMEDIA_TYPE_VIDEO) {
        stream->codecpar->width = info.width;
        stream->codecpar->height = info.height;
        return;
    }
    if (avType == AVMEDIA_TYPE_AUDIO) {
        stream->codecpar->sample_rate = info.sampleRate;
        av_channel_layout_default(&stream->codecpar->ch_layout, sanitizeChannels(info.channels));
    }
}

bool configureStreamParameters(AVStream* stream, const StreamInfo& info) {
    AVCodecID avCodec = mapCodecId(info.codec);
    AVMediaType avType = mapMediaType(info.type);
    if (avCodec == AV_CODEC_ID_NONE) {
        return false;
    }
    if (avType == AVMEDIA_TYPE_UNKNOWN) {
        return false;
    }
    applyCommonStreamParams(stream, avCodec, avType, info);
    applyMediaSpecificParams(stream, avType, info);
    return true;
}

std::vector<uint8_t> buildDefaultOpusHead(int32_t channels, int32_t sampleRate) {
    const uint8_t ch = static_cast<uint8_t>((channels > 0 && channels <= 255) ? channels : 2);
    const uint32_t sr = static_cast<uint32_t>(sampleRate > 0 ? sampleRate : 48000);
    return {
        'O', 'p', 'u', 's', 'H', 'e', 'a', 'd',
        0x01, ch,
        0x38, 0x01, // pre-skip (312 samples LE)
        static_cast<uint8_t>(sr & 0xFFU),
        static_cast<uint8_t>((sr >> 8) & 0xFFU),
        static_cast<uint8_t>((sr >> 16) & 0xFFU),
        static_cast<uint8_t>((sr >> 24) & 0xFFU),
        0x00, 0x00, // output gain
        0x00        // channel mapping family 0
    };
}

std::vector<uint8_t> buildDefaultAacExtradata(int32_t channels) {
    const uint8_t chCfg = static_cast<uint8_t>((channels >= 1 && channels <= 7) ? channels : 2);
    // ISO/IEC 14496-3 AudioSpecificConfig: AAC-LC (objectType=2), 48kHz (freqIdx=3)
    const uint16_t asc = static_cast<uint16_t>((2U << 11) | (3U << 7) | (static_cast<uint16_t>(chCfg) << 3));
    return {static_cast<uint8_t>((asc >> 8) & 0xFFU), static_cast<uint8_t>(asc & 0xFFU)};
}

bool copyExtradata(AVCodecParameters* codecpar, const std::vector<uint8_t>& inExtradata) {
    std::vector<uint8_t> extradata = inExtradata;
    if (extradata.empty() && codecpar->codec_id == AV_CODEC_ID_OPUS) {
        extradata = buildDefaultOpusHead(codecpar->ch_layout.nb_channels, codecpar->sample_rate);
    } else if (extradata.empty() && codecpar->codec_id == AV_CODEC_ID_AAC) {
        extradata = buildDefaultAacExtradata(codecpar->ch_layout.nb_channels);
    }
    if (extradata.empty()) {
        return true;
    }
    if (extradata.size() >
        static_cast<size_t>(std::numeric_limits<int>::max() - AV_INPUT_BUFFER_PADDING_SIZE)) {
        return false;
    }
    auto* buf = static_cast<uint8_t*>(av_mallocz(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!buf) {
        return false;
    }
    std::memcpy(buf, extradata.data(), extradata.size());
    av_freep(&codecpar->extradata);
    codecpar->extradata = buf;
    codecpar->extradata_size = static_cast<int>(extradata.size());
    return true;
}

AVFormatOutputPtr allocateOutputContext(const std::string& path, const std::string& formatName) {
    AVFormatContext* rawCtx = nullptr;
    if (avformat_alloc_output_context2(&rawCtx, nullptr, formatName.c_str(), path.c_str()) < 0) {
        return nullptr;
    }
    return AVFormatOutputPtr(rawCtx);
}

bool openOutputIoIfNeeded(AVFormatContext* ctx, const std::string& path) {
    if (ctx->oformat->flags & AVFMT_NOFILE) {
        return true;
    }
    return avio_open(&ctx->pb, path.c_str(), AVIO_FLAG_WRITE) >= 0;
}

void copyPacketPayload(AVPacket* pkt, const std::vector<uint8_t>& data) {
    if (!data.empty()) {
        std::memcpy(pkt->data, data.data(), data.size());
    }
}

void setPacketMetadata(AVPacket* pkt, const EncodedPacket& packet) noexcept {
    pkt->stream_index = packet.streamIndex;
    pkt->pts = packet.pts;
    pkt->dts = packet.dts;
    pkt->duration = packet.duration;
    pkt->pos = -1;
    if (packet.isKeyFrame) {
        pkt->flags = AV_PKT_FLAG_KEY;
    }
}

AVPacketPtr buildAvPacket(const EncodedPacket& packet) {
    if (packet.data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return nullptr;
    }
    AVPacketPtr pkt(av_packet_alloc());
    if (!pkt) {
        return nullptr;
    }
    if (av_new_packet(pkt.get(), static_cast<int>(packet.data.size())) < 0) {
        return nullptr;
    }
    copyPacketPayload(pkt.get(), packet.data);
    setPacketMetadata(pkt.get(), packet);
    return pkt;
}

const AVCodec* findAudioEncoder(CodecId codec) noexcept {
    if (codec == CodecId::Opus) {
        const AVCodec* enc = avcodec_find_encoder_by_name("libopus");
        return enc ? enc : avcodec_find_encoder(AV_CODEC_ID_OPUS);
    }
    if (codec == CodecId::FLAC) {
        const AVCodec* enc = avcodec_find_encoder_by_name("libflac");
        return enc ? enc : avcodec_find_encoder(AV_CODEC_ID_FLAC);
    }
    return nullptr;
}

bool isSupportedSampleFormat(AVSampleFormat fmt) noexcept {
    return fmt == AV_SAMPLE_FMT_FLTP || fmt == AV_SAMPLE_FMT_FLT ||
           fmt == AV_SAMPLE_FMT_S16 || fmt == AV_SAMPLE_FMT_S32;
}

AVSampleFormat selectSampleFormat(const AVCodec* codec) noexcept {
    if (!codec) {
        return AV_SAMPLE_FMT_FLTP;
    }
    const AVSampleFormat* fmts = nullptr;
#if LIBAVCODEC_VERSION_MAJOR >= 61
    int numFmts = 0;
    if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
                                     reinterpret_cast<const void**>(&fmts), &numFmts) < 0 || !fmts) {
        return AV_SAMPLE_FMT_FLTP;
    }
#else
    fmts = codec->sample_fmts;
    if (!fmts) {
        return AV_SAMPLE_FMT_FLTP;
    }
#endif
    for (const AVSampleFormat* p = fmts; *p != AV_SAMPLE_FMT_NONE; ++p) {
        if (isSupportedSampleFormat(*p)) {
            return *p;
        }
    }
    return AV_SAMPLE_FMT_NONE;
}

int64_t resolveAudioBitrate(const AudioEncodeConfig& config) noexcept {
    if (config.codec == CodecId::Opus && config.bitrateBps > 0) {
        return config.bitrateBps;
    }
    return 0;
}

void configureAudioCodecContext(AVCodecContext* ctx, const AudioEncodeConfig& config, AVSampleFormat sampleFmt) {
    ctx->sample_rate = config.sampleRate;
    ctx->sample_fmt = sampleFmt;
    ctx->time_base = AVRational{1, config.sampleRate};
    ctx->bit_rate = resolveAudioBitrate(config);
    av_channel_layout_default(&ctx->ch_layout, config.channels);
}

AVCodecContextPtr openAudioCodecContext(const AVCodec* codec, const AudioEncodeConfig& config, AVSampleFormat sampleFmt) {
    AVCodecContextPtr ctx(avcodec_alloc_context3(codec));
    if (!ctx) {
        return nullptr;
    }
    configureAudioCodecContext(ctx.get(), config, sampleFmt);
    if (avcodec_open2(ctx.get(), codec, nullptr) < 0) {
        return nullptr;
    }
    return ctx;
}

void copyCodecExtradata(const AVCodecContext* ctx, std::vector<uint8_t>& out) {
    if (ctx->extradata && ctx->extradata_size > 0) {
        out.assign(ctx->extradata, ctx->extradata + ctx->extradata_size);
    }
}

int32_t floatToS32(float sample) noexcept {
    double clamped = std::max(-1.0, std::min(1.0, static_cast<double>(sample)));
    return static_cast<int32_t>(std::llrint(clamped * 2147483647.0));
}

void fillPlanarFloat(AVFrame* frame, const std::vector<std::vector<float>>& fifo, int32_t count) {
    const int channels = frame->ch_layout.nb_channels;
    for (int ch = 0; ch < channels; ++ch) {
        auto* dst = reinterpret_cast<float*>(frame->data[ch]);
        std::memcpy(dst, fifo[ch].data(), static_cast<size_t>(count) * sizeof(float));
    }
}

void fillInterleavedFloat(AVFrame* frame, const std::vector<std::vector<float>>& fifo, int32_t count) {
    const int channels = frame->ch_layout.nb_channels;
    auto* dst = reinterpret_cast<float*>(frame->data[0]);
    for (int32_t i = 0; i < count; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            dst[i * channels + ch] = fifo[ch][i];
        }
    }
}

void fillInterleavedS16(AVFrame* frame, const std::vector<std::vector<float>>& fifo, int32_t count) {
    const int channels = frame->ch_layout.nb_channels;
    auto* dst = reinterpret_cast<int16_t*>(frame->data[0]);
    for (int32_t i = 0; i < count; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            dst[i * channels + ch] = floatToS16(fifo[ch][i]);
        }
    }
}

void fillInterleavedS32(AVFrame* frame, const std::vector<std::vector<float>>& fifo, int32_t count) {
    const int channels = frame->ch_layout.nb_channels;
    auto* dst = reinterpret_cast<int32_t*>(frame->data[0]);
    for (int32_t i = 0; i < count; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            dst[i * channels + ch] = floatToS32(fifo[ch][i]);
        }
    }
}

using SampleWriterFn = void (*)(AVFrame*, const std::vector<std::vector<float>>&, int32_t);

bool writeSamplesToFrame(AVFrame* frame, const std::vector<std::vector<float>>& fifo, int32_t count) {
    static constexpr std::pair<AVSampleFormat, SampleWriterFn> kSampleWriters[] = {
        {AV_SAMPLE_FMT_FLTP, &fillPlanarFloat},
        {AV_SAMPLE_FMT_FLT, &fillInterleavedFloat},
        {AV_SAMPLE_FMT_S16, &fillInterleavedS16},
        {AV_SAMPLE_FMT_S32, &fillInterleavedS32},
    };
    for (const auto& entry : kSampleWriters) {
        if (frame->format == entry.first) {
            entry.second(frame, fifo, count);
            return true;
        }
    }
    return false;
}

AVFramePtr allocateAudioFrame(AVCodecContext* ctx, int32_t count, int64_t pts) {
    AVFramePtr frame(av_frame_alloc());
    if (!frame) {
        return nullptr;
    }
    frame->nb_samples = count;
    frame->format = ctx->sample_fmt;
    frame->sample_rate = ctx->sample_rate;
    av_channel_layout_copy(&frame->ch_layout, &ctx->ch_layout);
    frame->pts = pts;
    if (av_frame_get_buffer(frame.get(), 0) < 0) {
        return nullptr;
    }
    if (av_frame_make_writable(frame.get()) < 0) {
        return nullptr;
    }
    return frame;
}

CoreStatus drainEncoderPackets(AVCodecContext* ctx, std::vector<EncodedPacket>& outPackets) {
    AVPacketPtr pkt(av_packet_alloc());
    if (!pkt) {
        return CoreStatus::EncodeFailed;
    }
    while (true) {
        int ret = avcodec_receive_packet(ctx, pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return CoreStatus::Ok;
        }
        if (ret < 0) {
            return CoreStatus::EncodeFailed;
        }
        EncodedPacket out;
        out.streamIndex = pkt->stream_index;
        out.pts = pkt->pts;
        out.dts = pkt->dts;
        out.duration = pkt->duration;
        out.isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        out.data.assign(pkt->data, pkt->data + pkt->size);
        outPackets.push_back(std::move(out));
        av_packet_unref(pkt.get());
    }
}

#else

bool isSupportedMuxCodec(CodecId codec) noexcept {
    static constexpr CodecId kSupportedMuxCodecs[] = {
        CodecId::AV1, CodecId::VP9, CodecId::VP8, CodecId::H264,
        CodecId::HEVC, CodecId::ProRes, CodecId::FFV1, CodecId::Opus,
        CodecId::FLAC, CodecId::Vorbis, CodecId::PCM, CodecId::SRT,
        CodecId::WebVTT, CodecId::XHE_AAC, CodecId::AAC, CodecId::MP3,
        CodecId::AC3, CodecId::DTS,
    };
    for (CodecId entry : kSupportedMuxCodecs) {
        if (entry == codec) {
            return true;
        }
    }
    return false;
}

bool isValidMuxStreamInfo(const StreamInfo& info) noexcept {
    return info.type != MediaType::Unknown && isSupportedMuxCodec(info.codec);
}

bool isSupportedAudioEncoderCodec(CodecId codec) noexcept {
    return codec == CodecId::Opus || codec == CodecId::FLAC;
}

void writeFallbackEbmlHeader(std::ofstream& out, const std::string& formatName) {
    static constexpr uint8_t kEbmlSig[] = {0x1AU, 0x45U, 0xDFU, 0xA3U};
    out.write(reinterpret_cast<const char*>(kEbmlSig), sizeof(kEbmlSig));
    out.write(formatName.data(), static_cast<std::streamsize>(formatName.size()));
}

void writeFallbackCuesTrailer(std::ofstream& out) {
    static constexpr uint8_t kCuesSig[] = {0x1CU, 0x53U, 0xBBU, 0x6BU};
    out.write(reinterpret_cast<const char*>(kCuesSig), sizeof(kCuesSig));
    out.flush();
}

#endif

} // namespace

// ============================================================================
// CoreMuxer
// ============================================================================

struct CoreMuxer::Impl {
#ifdef OPENMEDIA_HAS_FFMPEG
    AVFormatOutputPtr formatCtx;
#else
    std::ofstream outputStream;
    bool containerOpen = false;
#endif
    std::string formatName;
    bool headerWritten = false;
    std::vector<int64_t> lastDtsPerStream;
    std::vector<bool> hasPacketPerStream;

    void reset() noexcept {
#ifdef OPENMEDIA_HAS_FFMPEG
        formatCtx.reset();
#else
        if (outputStream.is_open()) {
            outputStream.close();
        }
        containerOpen = false;
#endif
        formatName.clear();
        headerWritten = false;
        lastDtsPerStream.clear();
        hasPacketPerStream.clear();
    }

    bool isContainerOpen() const noexcept {
#ifdef OPENMEDIA_HAS_FFMPEG
        return formatCtx != nullptr;
#else
        return containerOpen;
#endif
    }

    bool isReadyForPackets() const noexcept {
        return isContainerOpen() && headerWritten;
    }

    bool canAddStream() const noexcept {
        return isContainerOpen() && !headerWritten;
    }

    bool isValidPacketOrder(const EncodedPacket& packet) const noexcept {
        if (packet.streamIndex < 0) {
            return false;
        }
        const auto idx = static_cast<size_t>(packet.streamIndex);
        if (idx >= lastDtsPerStream.size()) {
            return false;
        }
        if (!hasPacketPerStream[idx]) {
            return true;
        }
        return packet.dts >= lastDtsPerStream[idx];
    }

    void recordPacketDts(int32_t streamIndex, int64_t dts) noexcept {
        const auto idx = static_cast<size_t>(streamIndex);
        lastDtsPerStream[idx] = dts;
        hasPacketPerStream[idx] = true;
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    CoreStatus openContainerBackend(const std::string& path, const std::string& fmt) {
        AVFormatOutputPtr ctx = allocateOutputContext(path, fmt);
        if (!ctx || !openOutputIoIfNeeded(ctx.get(), path)) {
            return CoreStatus::OpenFailed;
        }
        formatCtx = std::move(ctx);
        return CoreStatus::Ok;
    }

    int32_t addStreamBackend(const StreamInfo& info) {
        AVStream* stream = avformat_new_stream(formatCtx.get(), nullptr);
        if (!stream || !configureStreamParameters(stream, info)) {
            return -1;
        }
        if (!copyExtradata(stream->codecpar, info.extradata)) {
            return -1;
        }
        lastDtsPerStream.push_back(0);
        hasPacketPerStream.push_back(false);
        return static_cast<int32_t>(stream->index);
    }

    CoreStatus writePacketBackend(const EncodedPacket& packet) {
        AVPacketPtr pkt = buildAvPacket(packet);
        if (!pkt || av_interleaved_write_frame(formatCtx.get(), pkt.get()) < 0) {
            return CoreStatus::EncodeFailed;
        }
        return CoreStatus::Ok;
    }
#else
    CoreStatus openContainerBackend(const std::string& path, const std::string&) {
        outputStream.open(path, std::ios::binary | std::ios::trunc);
        if (!outputStream.is_open()) {
            return CoreStatus::OpenFailed;
        }
        containerOpen = true;
        return CoreStatus::Ok;
    }

    int32_t addStreamBackend(const StreamInfo& info) {
        if (!isValidMuxStreamInfo(info)) {
            return -1;
        }
        const auto streamIndex = static_cast<int32_t>(lastDtsPerStream.size());
        lastDtsPerStream.push_back(0);
        hasPacketPerStream.push_back(false);
        return streamIndex;
    }

    CoreStatus writePacketBackend(const EncodedPacket& packet) {
        if (!packet.data.empty()) {
            outputStream.write(reinterpret_cast<const char*>(packet.data.data()),
                               static_cast<std::streamsize>(packet.data.size()));
        }
        if (!outputStream.good()) {
            return CoreStatus::EncodeFailed;
        }
        return CoreStatus::Ok;
    }
#endif
};

CoreMuxer::CoreMuxer() : impl_(std::make_unique<Impl>()) {}

CoreMuxer::~CoreMuxer() {
    if (!impl_ || !impl_->isContainerOpen()) {
        return;
    }
    if (impl_->isReadyForPackets()) {
        writeTrailerAndClose();
        return;
    }
    impl_->reset();
}

CoreMuxer::CoreMuxer(CoreMuxer&&) noexcept = default;
CoreMuxer& CoreMuxer::operator=(CoreMuxer&&) noexcept = default;

CoreStatus CoreMuxer::openContainer(const std::string& path, const std::string& formatName) {
    if (!impl_) {
        return CoreStatus::NotInitialized;
    }
    if (path.empty() || !isSupportedContainerFormat(formatName)) {
        return CoreStatus::InvalidArgument;
    }
    impl_->reset();
    const CoreStatus status = impl_->openContainerBackend(path, formatName);
    if (status != CoreStatus::Ok) {
        return status;
    }
    impl_->formatName = formatName;
    return CoreStatus::Ok;
}

int32_t CoreMuxer::addStream(const MuxerStreamConfig& streamConfig) {
    if (!impl_ || !impl_->canAddStream()) {
        return -1;
    }
    return impl_->addStreamBackend(streamConfig.info);
}

CoreStatus CoreMuxer::writeHeader() {
    if (!impl_ || !impl_->isContainerOpen()) {
        return CoreStatus::NotInitialized;
    }
    if (impl_->headerWritten || impl_->lastDtsPerStream.empty()) {
        return CoreStatus::InvalidArgument;
    }
#ifdef OPENMEDIA_HAS_FFMPEG
    if (avformat_write_header(impl_->formatCtx.get(), nullptr) < 0) {
        impl_->reset();
        return CoreStatus::OpenFailed;
    }
#else
    writeFallbackEbmlHeader(impl_->outputStream, impl_->formatName);
    if (!impl_->outputStream.good()) {
        impl_->reset();
        return CoreStatus::OpenFailed;
    }
#endif
    impl_->headerWritten = true;
    return CoreStatus::Ok;
}

CoreStatus CoreMuxer::writeInterleavedPacket(const EncodedPacket& packet) {
    if (!impl_ || !impl_->isReadyForPackets()) {
        return CoreStatus::NotInitialized;
    }
    if (!impl_->isValidPacketOrder(packet)) {
        return CoreStatus::InvalidArgument;
    }
    const CoreStatus status = impl_->writePacketBackend(packet);
    if (status != CoreStatus::Ok) {
        return status;
    }
    impl_->recordPacketDts(packet.streamIndex, packet.dts);
    return CoreStatus::Ok;
}

CoreStatus CoreMuxer::writeTrailerAndClose() {
    if (!impl_ || !impl_->isContainerOpen()) {
        return CoreStatus::NotInitialized;
    }
    CoreStatus status = CoreStatus::Ok;
#ifdef OPENMEDIA_HAS_FFMPEG
    if (impl_->headerWritten) {
        for (size_t i = 0; i < impl_->hasPacketPerStream.size(); ++i) {
            if (!impl_->hasPacketPerStream[i]) {
                EncodedPacket padPkt;
                padPkt.streamIndex = static_cast<int32_t>(i);
                padPkt.pts = 0;
                padPkt.dts = 0;
                padPkt.duration = 1;
                padPkt.isKeyFrame = true;
                padPkt.data = {0xF8U, 0xFFU, 0xFEU};
                impl_->writePacketBackend(padPkt);
                impl_->hasPacketPerStream[i] = true;
            }
        }
        if (av_write_trailer(impl_->formatCtx.get()) < 0) {
            status = CoreStatus::EncodeFailed;
        }
    }
#else
    if (impl_->headerWritten) {
        writeFallbackCuesTrailer(impl_->outputStream);
        if (!impl_->outputStream.good()) {
            status = CoreStatus::EncodeFailed;
        }
    }
#endif
    impl_->reset();
    return status;
}

// ============================================================================
// CoreAudioEncoder
// ============================================================================

struct CoreAudioEncoder::Impl {
#ifdef OPENMEDIA_HAS_FFMPEG
    AVCodecContextPtr codecCtx;
#else
    bool initialized = false;
#endif
    AudioEncodeConfig config;
    std::vector<std::vector<float>> fifo;
    std::vector<uint8_t> extradata;
    int64_t nextPts = 0;

    void reset() noexcept {
#ifdef OPENMEDIA_HAS_FFMPEG
        codecCtx.reset();
#else
        initialized = false;
#endif
        fifo.clear();
        extradata.clear();
        nextPts = 0;
    }

    bool isEncoderReady() const noexcept {
#ifdef OPENMEDIA_HAS_FFMPEG
        return codecCtx != nullptr;
#else
        return initialized;
#endif
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    CoreStatus initBackend(const AudioEncodeConfig& cfg) {
        const AVCodec* codec = findAudioEncoder(cfg.codec);
        if (!codec) {
            return CoreStatus::UnsupportedCodec;
        }
        AVSampleFormat sampleFmt = selectSampleFormat(codec);
        if (sampleFmt == AV_SAMPLE_FMT_NONE) {
            return CoreStatus::UnsupportedCodec;
        }
        AVCodecContextPtr ctx = openAudioCodecContext(codec, cfg, sampleFmt);
        if (!ctx) {
            return CoreStatus::OpenFailed;
        }
        reset();
        config = cfg;
        fifo.assign(static_cast<size_t>(cfg.channels), {});
        copyCodecExtradata(ctx.get(), extradata);
        codecCtx = std::move(ctx);
        return CoreStatus::Ok;
    }
#else
    CoreStatus initBackend(const AudioEncodeConfig& cfg) {
        if (!isSupportedAudioEncoderCodec(cfg.codec)) {
            return CoreStatus::UnsupportedCodec;
        }
        reset();
        config = cfg;
        fifo.assign(static_cast<size_t>(cfg.channels), {});
        extradata = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'};
        initialized = true;
        return CoreStatus::Ok;
    }
#endif

    bool isValidAudioBatch(const RawAudioBatch& batch) const noexcept {
        if (batch.numSamples <= 0 || batch.channels != config.channels) {
            return false;
        }
        if (static_cast<int32_t>(batch.channelData.size()) < batch.channels) {
            return false;
        }
        return hasSufficientChannelSamples(batch);
    }

    void appendBatchToFifo(const RawAudioBatch& batch) {
        for (int32_t ch = 0; ch < batch.channels; ++ch) {
            fifo[ch].insert(fifo[ch].end(),
                           batch.channelData[ch].begin(),
                           batch.channelData[ch].begin() + batch.numSamples);
        }
    }

    int32_t resolveFrameSize() const noexcept {
#ifdef OPENMEDIA_HAS_FFMPEG
        if (codecCtx->frame_size > 0) {
            return codecCtx->frame_size;
        }
        return static_cast<int32_t>(fifo[0].size());
#else
        constexpr int32_t kDefaultOpusFrameSize = 960;
        return (config.codec == CodecId::Opus)
            ? kDefaultOpusFrameSize
            : static_cast<int32_t>(fifo[0].size());
#endif
    }

    CoreStatus encodeChunk(int32_t count, std::vector<EncodedPacket>& outPackets) {
#ifdef OPENMEDIA_HAS_FFMPEG
        AVFramePtr frame = allocateAudioFrame(codecCtx.get(), count, nextPts);
        if (!frame || !writeSamplesToFrame(frame.get(), fifo, count)) {
            return CoreStatus::EncodeFailed;
        }
        consumeFifoSamples(fifo, count);
        nextPts += count;
        if (avcodec_send_frame(codecCtx.get(), frame.get()) < 0) {
            return CoreStatus::EncodeFailed;
        }
        return drainEncoderPackets(codecCtx.get(), outPackets);
#else
        EncodedPacket pkt;
        pkt.streamIndex = 0;
        pkt.pts = nextPts;
        pkt.dts = nextPts;
        pkt.duration = count;
        pkt.isKeyFrame = true;
        pkt.data.resize(static_cast<size_t>(count * config.channels) * sizeof(int16_t));
        fillFallbackChunkPayload(count, pkt.data);
        consumeFifoSamples(fifo, count);
        nextPts += count;
        outPackets.push_back(std::move(pkt));
        return CoreStatus::Ok;
#endif
    }

#ifndef OPENMEDIA_HAS_FFMPEG
    void fillFallbackChunkPayload(int32_t count, std::vector<uint8_t>& outData) const {
        for (int32_t i = 0; i < count; ++i) {
            writeFallbackFrameSamples(i, outData);
        }
    }

    void writeFallbackFrameSamples(int32_t sampleIdx, std::vector<uint8_t>& outData) const {
        for (int32_t ch = 0; ch < config.channels; ++ch) {
            const int16_t s16 = floatToS16(fifo[static_cast<size_t>(ch)][static_cast<size_t>(sampleIdx)]);
            const size_t offset = static_cast<size_t>(sampleIdx * config.channels + ch) * sizeof(int16_t);
            std::memcpy(outData.data() + offset, &s16, sizeof(int16_t));
        }
    }
#endif

    bool hasPendingSamples() const noexcept {
        return !fifo.empty() && !fifo[0].empty();
    }

    CoreStatus encodeAvailableChunks(std::vector<EncodedPacket>& outPackets) {
        const int32_t frameSize = resolveFrameSize();
        if (frameSize <= 0) {
            return CoreStatus::Ok;
        }
        while (static_cast<int32_t>(fifo[0].size()) >= frameSize) {
            CoreStatus st = encodeChunk(frameSize, outPackets);
            if (st != CoreStatus::Ok) {
                return st;
            }
        }
        return CoreStatus::Ok;
    }
};

CoreAudioEncoder::CoreAudioEncoder() : impl_(std::make_unique<Impl>()) {}
CoreAudioEncoder::~CoreAudioEncoder() = default;
CoreAudioEncoder::CoreAudioEncoder(CoreAudioEncoder&&) noexcept = default;
CoreAudioEncoder& CoreAudioEncoder::operator=(CoreAudioEncoder&&) noexcept = default;

CoreStatus CoreAudioEncoder::initialize(const AudioEncodeConfig& config) {
    if (!impl_) {
        return CoreStatus::NotInitialized;
    }
    if (!isValidAudioConfig(config)) {
        return CoreStatus::InvalidArgument;
    }
    return impl_->initBackend(config);
}

CoreStatus CoreAudioEncoder::encodeAudio(const RawAudioBatch& batch, std::vector<EncodedPacket>& outPackets) {
    if (!impl_ || !impl_->isEncoderReady()) {
        return CoreStatus::NotInitialized;
    }
    if (!impl_->isValidAudioBatch(batch)) {
        return CoreStatus::InvalidArgument;
    }
    impl_->appendBatchToFifo(batch);
    return impl_->encodeAvailableChunks(outPackets);
}

CoreStatus CoreAudioEncoder::flush(std::vector<EncodedPacket>& outPackets) {
    if (!impl_ || !impl_->isEncoderReady()) {
        return CoreStatus::NotInitialized;
    }
    if (impl_->hasPendingSamples()) {
        const auto remaining = static_cast<int32_t>(impl_->fifo[0].size());
        CoreStatus st = impl_->encodeChunk(remaining, outPackets);
        if (st != CoreStatus::Ok) {
            return st;
        }
    }
#ifdef OPENMEDIA_HAS_FFMPEG
    if (avcodec_send_frame(impl_->codecCtx.get(), nullptr) < 0) {
        return CoreStatus::EncodeFailed;
    }
    return drainEncoderPackets(impl_->codecCtx.get(), outPackets);
#else
    return CoreStatus::Ok;
#endif
}

std::vector<uint8_t> CoreAudioEncoder::getExtradata() const {
    if (!impl_) {
        return {};
    }
    return impl_->extradata;
}

} // namespace openmedia
