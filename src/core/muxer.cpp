#include "openmedia/muxer.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace openmedia {

namespace {

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
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

bool isSupportedContainerFormat(const std::string& formatName) {
    return formatName == "matroska" || formatName == "webm" || formatName == "mp4";
}

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
    };
    for (const auto& entry : kCodecMap) {
        if (entry.first == codec) {
            return entry.second;
        }
    }
    return AV_CODEC_ID_NONE;
}

AVMediaType mapMediaType(MediaType type) noexcept {
    if (type == MediaType::Video) {
        return AVMEDIA_TYPE_VIDEO;
    }
    if (type == MediaType::Audio) {
        return AVMEDIA_TYPE_AUDIO;
    }
    if (type == MediaType::Subtitle) {
        return AVMEDIA_TYPE_SUBTITLE;
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

bool copyExtradata(AVCodecParameters* codecpar, const std::vector<uint8_t>& extradata) {
    if (extradata.empty()) {
        return true;
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
    if (packet.isKeyFrame) {
        pkt->flags = AV_PKT_FLAG_KEY;
    }
}

AVPacketPtr buildAvPacket(const EncodedPacket& packet) {
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
    if (!codec || !codec->sample_fmts) {
        return AV_SAMPLE_FMT_FLTP;
    }
    for (const AVSampleFormat* p = codec->sample_fmts; *p != AV_SAMPLE_FMT_NONE; ++p) {
        if (isSupportedSampleFormat(*p)) {
            return *p;
        }
    }
    return AV_SAMPLE_FMT_NONE;
}

bool isValidAudioConfig(const AudioEncodeConfig& config) noexcept {
    return config.sampleRate > 0 && config.channels > 0;
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

bool hasSufficientChannelSamples(const RawAudioBatch& batch) noexcept {
    for (int32_t ch = 0; ch < batch.channels; ++ch) {
        if (static_cast<int32_t>(batch.channelData[ch].size()) < batch.numSamples) {
            return false;
        }
    }
    return true;
}

int16_t floatToS16(float sample) noexcept {
    float clamped = std::max(-1.0f, std::min(1.0f, sample));
    return static_cast<int16_t>(std::lrintf(clamped * 32767.0f));
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

bool writeSamplesToFrame(AVFrame* frame, const std::vector<std::vector<float>>& fifo, int32_t count) {
    if (frame->format == AV_SAMPLE_FMT_FLTP) {
        fillPlanarFloat(frame, fifo, count);
        return true;
    }
    if (frame->format == AV_SAMPLE_FMT_FLT) {
        fillInterleavedFloat(frame, fifo, count);
        return true;
    }
    if (frame->format == AV_SAMPLE_FMT_S16) {
        fillInterleavedS16(frame, fifo, count);
        return true;
    }
    if (frame->format == AV_SAMPLE_FMT_S32) {
        fillInterleavedS32(frame, fifo, count);
        return true;
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

void consumeFifoSamples(std::vector<std::vector<float>>& fifo, int32_t count) {
    for (auto& chBuf : fifo) {
        chBuf.erase(chBuf.begin(), chBuf.begin() + count);
    }
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
        out.pts = pkt->pts;
        out.dts = pkt->dts;
        out.duration = pkt->duration;
        out.isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        out.data.assign(pkt->data, pkt->data + pkt->size);
        outPackets.push_back(std::move(out));
        av_packet_unref(pkt.get());
    }
}

} // namespace

// ============================================================================
// CoreMuxer
// ============================================================================

struct CoreMuxer::Impl {
    AVFormatOutputPtr formatCtx;
    std::string formatName;
    bool headerWritten = false;
    std::vector<int64_t> lastDtsPerStream;
    std::vector<bool> hasPacketPerStream;

    void reset() noexcept {
        formatCtx.reset();
        formatName.clear();
        headerWritten = false;
        lastDtsPerStream.clear();
        hasPacketPerStream.clear();
    }

    bool isReadyForPackets() const noexcept {
        return formatCtx != nullptr && headerWritten;
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
};

CoreMuxer::CoreMuxer() : impl_(std::make_unique<Impl>()) {}

CoreMuxer::~CoreMuxer() {
    if (impl_ && impl_->isReadyForPackets()) {
        writeTrailerAndClose();
    }
}

CoreMuxer::CoreMuxer(CoreMuxer&&) noexcept = default;
CoreMuxer& CoreMuxer::operator=(CoreMuxer&&) noexcept = default;

CoreStatus CoreMuxer::openContainer(const std::string& path, const std::string& formatName) {
    if (path.empty()) {
        return CoreStatus::InvalidArgument;
    }
    if (!isSupportedContainerFormat(formatName)) {
        return CoreStatus::InvalidArgument;
    }
    impl_->reset();
    AVFormatOutputPtr ctx = allocateOutputContext(path, formatName);
    if (!ctx) {
        return CoreStatus::OpenFailed;
    }
    if (!openOutputIoIfNeeded(ctx.get(), path)) {
        return CoreStatus::OpenFailed;
    }
    impl_->formatCtx = std::move(ctx);
    impl_->formatName = formatName;
    return CoreStatus::Ok;
}

int32_t CoreMuxer::addStream(const MuxerStreamConfig& streamConfig) {
    if (!impl_->formatCtx || impl_->headerWritten) {
        return -1;
    }
    AVStream* stream = avformat_new_stream(impl_->formatCtx.get(), nullptr);
    if (!stream) {
        return -1;
    }
    if (!configureStreamParameters(stream, streamConfig.info)) {
        return -1;
    }
    if (!copyExtradata(stream->codecpar, streamConfig.info.extradata)) {
        return -1;
    }
    impl_->lastDtsPerStream.push_back(0);
    impl_->hasPacketPerStream.push_back(false);
    return static_cast<int32_t>(stream->index);
}

CoreStatus CoreMuxer::writeHeader() {
    if (!impl_->formatCtx) {
        return CoreStatus::NotInitialized;
    }
    if (impl_->headerWritten || impl_->formatCtx->nb_streams == 0) {
        return CoreStatus::InvalidArgument;
    }
    if (avformat_write_header(impl_->formatCtx.get(), nullptr) < 0) {
        return CoreStatus::OpenFailed;
    }
    impl_->headerWritten = true;
    return CoreStatus::Ok;
}

CoreStatus CoreMuxer::writeInterleavedPacket(const EncodedPacket& packet) {
    if (!impl_->isReadyForPackets()) {
        return CoreStatus::NotInitialized;
    }
    if (!impl_->isValidPacketOrder(packet)) {
        return CoreStatus::InvalidArgument;
    }
    AVPacketPtr pkt = buildAvPacket(packet);
    if (!pkt) {
        return CoreStatus::EncodeFailed;
    }
    if (av_interleaved_write_frame(impl_->formatCtx.get(), pkt.get()) < 0) {
        return CoreStatus::EncodeFailed;
    }
    impl_->recordPacketDts(packet.streamIndex, packet.dts);
    return CoreStatus::Ok;
}

CoreStatus CoreMuxer::writeTrailerAndClose() {
    if (!impl_->formatCtx) {
        return CoreStatus::NotInitialized;
    }
    CoreStatus status = CoreStatus::Ok;
    if (impl_->headerWritten && av_write_trailer(impl_->formatCtx.get()) < 0) {
        status = CoreStatus::EncodeFailed;
    }
    impl_->reset();
    return status;
}

// ============================================================================
// CoreAudioEncoder
// ============================================================================

struct CoreAudioEncoder::Impl {
    AVCodecContextPtr codecCtx;
    AudioEncodeConfig config;
    std::vector<std::vector<float>> fifo;
    std::vector<uint8_t> extradata;
    int64_t nextPts = 0;

    void reset() noexcept {
        codecCtx.reset();
        fifo.clear();
        extradata.clear();
        nextPts = 0;
    }

    void applyInitializedState(const AudioEncodeConfig& cfg, AVCodecContextPtr ctx) {
        reset();
        config = cfg;
        fifo.assign(static_cast<size_t>(cfg.channels), {});
        copyCodecExtradata(ctx.get(), extradata);
        codecCtx = std::move(ctx);
    }

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
        if (codecCtx->frame_size > 0) {
            return codecCtx->frame_size;
        }
        return static_cast<int32_t>(fifo[0].size());
    }

    CoreStatus encodeChunk(int32_t count, std::vector<EncodedPacket>& outPackets) {
        AVFramePtr frame = allocateAudioFrame(codecCtx.get(), count, nextPts);
        if (!frame) {
            return CoreStatus::EncodeFailed;
        }
        if (!writeSamplesToFrame(frame.get(), fifo, count)) {
            return CoreStatus::EncodeFailed;
        }
        consumeFifoSamples(fifo, count);
        nextPts += count;
        if (avcodec_send_frame(codecCtx.get(), frame.get()) < 0) {
            return CoreStatus::EncodeFailed;
        }
        return drainEncoderPackets(codecCtx.get(), outPackets);
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
    if (!isValidAudioConfig(config)) {
        return CoreStatus::InvalidArgument;
    }
    const AVCodec* codec = findAudioEncoder(config.codec);
    if (!codec) {
        return CoreStatus::UnsupportedCodec;
    }
    AVSampleFormat sampleFmt = selectSampleFormat(codec);
    if (sampleFmt == AV_SAMPLE_FMT_NONE) {
        return CoreStatus::UnsupportedCodec;
    }
    AVCodecContextPtr ctx = openAudioCodecContext(codec, config, sampleFmt);
    if (!ctx) {
        return CoreStatus::OpenFailed;
    }
    impl_->applyInitializedState(config, std::move(ctx));
    return CoreStatus::Ok;
}

CoreStatus CoreAudioEncoder::encodeAudio(const RawAudioBatch& batch, std::vector<EncodedPacket>& outPackets) {
    if (!impl_->codecCtx) {
        return CoreStatus::NotInitialized;
    }
    if (!impl_->isValidAudioBatch(batch)) {
        return CoreStatus::InvalidArgument;
    }
    impl_->appendBatchToFifo(batch);
    return impl_->encodeAvailableChunks(outPackets);
}

CoreStatus CoreAudioEncoder::flush(std::vector<EncodedPacket>& outPackets) {
    if (!impl_->codecCtx) {
        return CoreStatus::NotInitialized;
    }
    if (!impl_->fifo.empty() && !impl_->fifo[0].empty()) {
        const auto remaining = static_cast<int32_t>(impl_->fifo[0].size());
        CoreStatus st = impl_->encodeChunk(remaining, outPackets);
        if (st != CoreStatus::Ok) {
            return st;
        }
    }
    if (avcodec_send_frame(impl_->codecCtx.get(), nullptr) < 0) {
        return CoreStatus::EncodeFailed;
    }
    return drainEncoderPackets(impl_->codecCtx.get(), outPackets);
}

std::vector<uint8_t> CoreAudioEncoder::getExtradata() const {
    return impl_->extradata;
}

} // namespace openmedia
