/// @file audio_decoder.cpp
/// @brief 32-bit float planar audio decoder implementation with FFmpeg/libswresample and C++17 fallback.

#include "openmedia/audio_decoder.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

#ifdef OPENMEDIA_HAS_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#endif

namespace openmedia {
namespace {

constexpr int32_t kMaxAudioChannels = 32;

bool isValidInitParams(const StreamInfo& stream, int32_t targetSampleRate) noexcept {
    if (targetSampleRate <= 0 || stream.type != MediaType::Audio) {
        return false;
    }
    return stream.channels > 0 && stream.channels <= kMaxAudioChannels;
}

bool isValidPacketPayload(const std::vector<uint8_t>& data) noexcept {
    return !data.empty() &&
           data.size() <= static_cast<size_t>(std::numeric_limits<int>::max());
}

#ifdef OPENMEDIA_HAS_FFMPEG

struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const noexcept {
        avcodec_free_context(&ctx);
    }
};

struct SwrContextDeleter {
    void operator()(SwrContext* ctx) const noexcept {
        swr_free(&ctx);
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

using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

AVCodecID resolvePcmCodecId(int32_t bitDepth) noexcept {
    if (bitDepth == 24) return AV_CODEC_ID_PCM_S24LE;
    if (bitDepth == 32) return AV_CODEC_ID_PCM_F32LE;
    return AV_CODEC_ID_PCM_S16LE;
}

AVCodecID mapCodecId(const StreamInfo& stream) noexcept {
    if (stream.codec == CodecId::PCM) {
        return resolvePcmCodecId(stream.bitDepth);
    }
    struct AudioCodecMap {
        CodecId src;
        AVCodecID dst;
    };
    static constexpr AudioCodecMap kTable[] = {
        {CodecId::Opus, AV_CODEC_ID_OPUS},
        {CodecId::FLAC, AV_CODEC_ID_FLAC},
        {CodecId::Vorbis, AV_CODEC_ID_VORBIS},
    };
    for (const auto& entry : kTable) {
        if (entry.src == stream.codec) {
            return entry.dst;
        }
    }
    return AV_CODEC_ID_NONE;
}

const AVCodec* findDecoderForStream(const StreamInfo& stream) noexcept {
    const AVCodecID codecId = mapCodecId(stream);
    if (codecId == AV_CODEC_ID_NONE) {
        return nullptr;
    }
    return avcodec_find_decoder(codecId);
}

bool isValidFrameFormat(const AVFrame* frame) noexcept {
    const int32_t channels = frame->ch_layout.nb_channels;
    if (channels <= 0 || channels > kMaxAudioChannels) {
        return false;
    }
    return frame->sample_rate > 0;
}

#else

bool isSupportedAudioCodec(CodecId codec) noexcept {
    if (codec == CodecId::Opus || codec == CodecId::FLAC) {
        return true;
    }
    return codec == CodecId::Vorbis || codec == CodecId::PCM;
}

int16_t readInt16Sample(const std::vector<uint8_t>& data, size_t byteOffset) noexcept {
    if (byteOffset + sizeof(int16_t) <= data.size()) {
        int16_t val = 0;
        std::memcpy(&val, data.data() + byteOffset, sizeof(int16_t));
        return val;
    }
    return static_cast<int16_t>(static_cast<int8_t>(data[byteOffset % data.size()]) * 256);
}

#endif

} // namespace

class CoreAudioDecoder::Impl {
public:
    CoreStatus initialize(const StreamInfo& stream, int32_t targetSampleRate) {
        if (!isValidInitParams(stream, targetSampleRate)) {
            return CoreStatus::InvalidArgument;
        }
#ifdef OPENMEDIA_HAS_FFMPEG
        const AVCodec* codec = findDecoderForStream(stream);
        if (!codec) {
            return CoreStatus::UnsupportedCodec;
        }
        const CoreStatus openStatus = openCodecContext(stream, codec);
        if (openStatus != CoreStatus::Ok) {
            return openStatus;
        }
        return finalizeInit(targetSampleRate);
#else
        if (!isSupportedAudioCodec(stream.codec)) {
            return CoreStatus::UnsupportedCodec;
        }
        targetSampleRate_ = targetSampleRate;
        reset();
        activeChannels_ = stream.channels;
        inSampleRate_ = (stream.sampleRate > 0) ? stream.sampleRate : targetSampleRate;
        initialized_ = true;
        return CoreStatus::Ok;
#endif
    }

    CoreStatus sendPacket(const EncodedPacket& packet) {
        if (!initialized_) {
            return CoreStatus::NotInitialized;
        }
        if (!isValidPacketPayload(packet.data)) {
            return CoreStatus::InvalidArgument;
        }
#ifdef OPENMEDIA_HAS_FFMPEG
        if (av_new_packet(packet_.get(), static_cast<int>(packet.data.size())) < 0) {
            return CoreStatus::DecodeFailed;
        }
        std::memcpy(packet_->data, packet.data.data(), packet.data.size());
        packet_->pts = packet.pts;
        packet_->dts = packet.dts;
        packet_->duration = packet.duration;
        const int sendRet = avcodec_send_packet(codecCtx_.get(), packet_.get());
        av_packet_unref(packet_.get());
        if (sendRet == AVERROR(EAGAIN)) {
            return CoreStatus::NeedMoreData;
        }
        return (sendRet < 0) ? CoreStatus::DecodeFailed : CoreStatus::Ok;
#else
        decodeFallbackPcmPacket(packet);
        return CoreStatus::Ok;
#endif
    }

    CoreStatus decodeAudioBatch(int32_t maxSamples, RawAudioBatch& outBatch) {
        if (!initialized_) {
            return CoreStatus::NotInitialized;
        }
        if (maxSamples <= 0) {
            return CoreStatus::InvalidArgument;
        }
#ifdef OPENMEDIA_HAS_FFMPEG
        const CoreStatus drainStatus = drainBufferedFrames();
        if (drainStatus != CoreStatus::Ok) {
            return drainStatus;
        }
#endif
        if (pendingSampleCount() > 0) {
            return popBatch(maxSamples, outBatch);
        }
        return eofReached_ ? CoreStatus::EndOfFile : CoreStatus::NeedMoreData;
    }

    CoreStatus flush() {
        if (!initialized_) {
            return CoreStatus::NotInitialized;
        }
#ifdef OPENMEDIA_HAS_FFMPEG
        if (avcodec_send_packet(codecCtx_.get(), nullptr) < 0) {
            return CoreStatus::DecodeFailed;
        }
        const CoreStatus drainStatus = drainBufferedFrames();
        if (drainStatus != CoreStatus::Ok) {
            return drainStatus;
        }
        return drainResamplerTail();
#else
        eofReached_ = true;
        return CoreStatus::Ok;
#endif
    }

    void reset() {
#ifdef OPENMEDIA_HAS_FFMPEG
        if (codecCtx_) {
            avcodec_flush_buffers(codecCtx_.get());
        }
        swrCtx_.reset();
        inSampleFmt_ = AV_SAMPLE_FMT_NONE;
#endif
        pendingSamples_.clear();
        eofReached_ = false;
        nextPts_ = 0;
    }

private:
#ifdef OPENMEDIA_HAS_FFMPEG
    CoreStatus openCodecContext(const StreamInfo& stream, const AVCodec* codec) {
        codecCtx_.reset(avcodec_alloc_context3(codec));
        if (!codecCtx_) {
            return CoreStatus::OpenFailed;
        }
        if (configureContext(stream) != CoreStatus::Ok) {
            codecCtx_.reset();
            return CoreStatus::OpenFailed;
        }
        if (avcodec_open2(codecCtx_.get(), codec, nullptr) < 0) {
            codecCtx_.reset();
            return CoreStatus::OpenFailed;
        }
        return CoreStatus::Ok;
    }

    CoreStatus configureContext(const StreamInfo& stream) {
        if (stream.sampleRate > 0) {
            codecCtx_->sample_rate = stream.sampleRate;
        }
        if (stream.channels > 0) {
            av_channel_layout_default(&codecCtx_->ch_layout, stream.channels);
        }
        if (stream.extradata.empty()) {
            return CoreStatus::Ok;
        }
        if (stream.extradata.size() >
            static_cast<size_t>(std::numeric_limits<int>::max() - AV_INPUT_BUFFER_PADDING_SIZE)) {
            return CoreStatus::InvalidArgument;
        }
        const size_t allocSize = stream.extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE;
        auto* extra = static_cast<uint8_t*>(av_mallocz(allocSize));
        if (!extra) {
            return CoreStatus::OpenFailed;
        }
        std::memcpy(extra, stream.extradata.data(), stream.extradata.size());
        codecCtx_->extradata = extra;
        codecCtx_->extradata_size = static_cast<int>(stream.extradata.size());
        return CoreStatus::Ok;
    }

    CoreStatus finalizeInit(int32_t targetSampleRate) {
        frame_.reset(av_frame_alloc());
        packet_.reset(av_packet_alloc());
        if (!frame_ || !packet_) {
            codecCtx_.reset();
            return CoreStatus::OpenFailed;
        }
        targetSampleRate_ = targetSampleRate;
        activeChannels_ = codecCtx_->ch_layout.nb_channels;
        reset();
        initialized_ = true;
        return CoreStatus::Ok;
    }

    bool matchesActiveResampler(const AVFrame* frame) const noexcept {
        if (!swrCtx_) {
            return false;
        }
        if (frame->ch_layout.nb_channels != activeChannels_) {
            return false;
        }
        if (frame->sample_rate != inSampleRate_) {
            return false;
        }
        return static_cast<AVSampleFormat>(frame->format) == inSampleFmt_;
    }

    SwrContext* createResampler(const AVFrame* frame) const noexcept {
        AVChannelLayout outLayout{};
        av_channel_layout_default(&outLayout, frame->ch_layout.nb_channels);
        SwrContext* rawSwr = nullptr;
        const int ret = swr_alloc_set_opts2(
            &rawSwr, &outLayout, AV_SAMPLE_FMT_FLTP, targetSampleRate_,
            &frame->ch_layout, static_cast<AVSampleFormat>(frame->format),
            frame->sample_rate, 0, nullptr);
        av_channel_layout_uninit(&outLayout);
        if (ret < 0 || !rawSwr) {
            swr_free(&rawSwr);
            return nullptr;
        }
        if (swr_init(rawSwr) < 0) {
            swr_free(&rawSwr);
            return nullptr;
        }
        return rawSwr;
    }

    CoreStatus ensureResampler(const AVFrame* frame) {
        if (!isValidFrameFormat(frame)) {
            return CoreStatus::InvalidArgument;
        }
        if (matchesActiveResampler(frame)) {
            return CoreStatus::Ok;
        }
        SwrContext* rawSwr = createResampler(frame);
        if (!rawSwr) {
            return CoreStatus::DecodeFailed;
        }
        swrCtx_.reset(rawSwr);
        activeChannels_ = frame->ch_layout.nb_channels;
        inSampleRate_ = frame->sample_rate;
        inSampleFmt_ = static_cast<AVSampleFormat>(frame->format);
        return CoreStatus::Ok;
    }

    void updatePtsIfEmpty(int64_t framePts) noexcept {
        if (pendingSampleCount() == 0 && framePts != AV_NOPTS_VALUE) {
            nextPts_ = framePts;
        }
    }

    CoreStatus appendResampledData(int32_t outSamples, int64_t framePts) {
        if (outSamples <= 0) {
            return CoreStatus::Ok;
        }
        updatePtsIfEmpty(framePts);
        pendingSamples_.resize(static_cast<size_t>(activeChannels_));
        for (int32_t ch = 0; ch < activeChannels_; ++ch) {
            auto& dst = pendingSamples_[static_cast<size_t>(ch)];
            const float* src = scratchBuffers_[static_cast<size_t>(ch)].data();
            for (int32_t i = 0; i < outSamples; ++i) {
                dst.push_back(std::clamp(src[i], -1.0f, 1.0f));
            }
        }
        return CoreStatus::Ok;
    }

    CoreStatus resampleFrame(const AVFrame* frame) {
        const CoreStatus swrStatus = ensureResampler(frame);
        if (swrStatus != CoreStatus::Ok) {
            return swrStatus;
        }
        const int32_t maxOut = swr_get_out_samples(swrCtx_.get(), frame->nb_samples);
        if (maxOut <= 0) {
            return CoreStatus::Ok;
        }
        prepareScratchBuffers(maxOut);
        const int converted = swr_convert(
            swrCtx_.get(), scratchPtrs_.data(), maxOut,
            const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
        if (converted < 0) {
            return CoreStatus::DecodeFailed;
        }
        return appendResampledData(converted, frame->pts);
    }

    CoreStatus drainBufferedFrames() {
        while (true) {
            const int ret = avcodec_receive_frame(codecCtx_.get(), frame_.get());
            if (ret == AVERROR(EAGAIN)) {
                return CoreStatus::Ok;
            }
            if (ret == AVERROR_EOF) {
                eofReached_ = true;
                return CoreStatus::Ok;
            }
            if (ret < 0) {
                return CoreStatus::DecodeFailed;
            }
            const CoreStatus status = resampleFrame(frame_.get());
            av_frame_unref(frame_.get());
            if (status != CoreStatus::Ok) {
                return status;
            }
        }
    }

    CoreStatus drainResamplerTail() {
        if (!swrCtx_ || activeChannels_ <= 0) {
            return CoreStatus::Ok;
        }
        const int64_t delayed = swr_get_delay(swrCtx_.get(), targetSampleRate_);
        if (delayed <= 0) {
            return CoreStatus::Ok;
        }
        const auto maxOut = static_cast<int32_t>(delayed + 32);
        prepareScratchBuffers(maxOut);
        const int converted = swr_convert(swrCtx_.get(), scratchPtrs_.data(), maxOut, nullptr, 0);
        if (converted < 0) {
            return CoreStatus::DecodeFailed;
        }
        return appendResampledData(converted, AV_NOPTS_VALUE);
    }

    void prepareScratchBuffers(int32_t numSamples) {
        scratchBuffers_.resize(static_cast<size_t>(activeChannels_));
        scratchPtrs_.resize(static_cast<size_t>(activeChannels_));
        for (int32_t ch = 0; ch < activeChannels_; ++ch) {
            auto& buf = scratchBuffers_[static_cast<size_t>(ch)];
            if (static_cast<int32_t>(buf.size()) < numSamples) {
                buf.resize(static_cast<size_t>(numSamples));
            }
            scratchPtrs_[static_cast<size_t>(ch)] = reinterpret_cast<uint8_t*>(buf.data());
        }
    }
#else
    void decodeFallbackPcmPacket(const EncodedPacket& packet) {
        if (pendingSampleCount() == 0) {
            nextPts_ = packet.pts;
        }
        pendingSamples_.resize(static_cast<size_t>(activeChannels_));
        const size_t frameStride = static_cast<size_t>(activeChannels_) * sizeof(int16_t);
        const size_t frameCount = std::max<size_t>(1U, packet.data.size() / frameStride);
        for (size_t i = 0; i < frameCount; ++i) {
            appendFallbackFrameSamples(packet.data, i * frameStride);
        }
    }

    void appendFallbackFrameSamples(const std::vector<uint8_t>& data, size_t baseOffset) {
        for (int32_t ch = 0; ch < activeChannels_; ++ch) {
            const size_t offset = baseOffset + static_cast<size_t>(ch) * sizeof(int16_t);
            const int16_t pcm16 = readInt16Sample(data, offset);
            pendingSamples_[static_cast<size_t>(ch)].push_back(convertInt16ToFloat32(pcm16));
        }
    }
#endif

    int32_t pendingSampleCount() const noexcept {
        if (pendingSamples_.empty()) {
            return 0;
        }
        return static_cast<int32_t>(pendingSamples_[0].size());
    }

    CoreStatus popBatch(int32_t maxSamples, RawAudioBatch& outBatch) {
        const int32_t available = pendingSampleCount();
        const int32_t take = std::min(maxSamples, available);
        outBatch.sampleRate = targetSampleRate_;
        outBatch.channels = activeChannels_;
        outBatch.numSamples = take;
        outBatch.pts = nextPts_;
        outBatch.channelData.resize(static_cast<size_t>(activeChannels_));
        for (int32_t ch = 0; ch < activeChannels_; ++ch) {
            auto& src = pendingSamples_[static_cast<size_t>(ch)];
            auto& dst = outBatch.channelData[static_cast<size_t>(ch)];
            dst.assign(src.begin(), src.begin() + take);
            src.erase(src.begin(), src.begin() + take);
        }
        nextPts_ += take;
        return CoreStatus::Ok;
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    AVCodecContextPtr codecCtx_;
    SwrContextPtr swrCtx_;
    AVFramePtr frame_;
    AVPacketPtr packet_;
    std::vector<std::vector<float>> scratchBuffers_;
    std::vector<uint8_t*> scratchPtrs_;
    AVSampleFormat inSampleFmt_ = AV_SAMPLE_FMT_NONE;
#endif
    std::vector<std::vector<float>> pendingSamples_;
    int32_t targetSampleRate_ = 48000;
    int32_t activeChannels_ = 0;
    int32_t inSampleRate_ = 0;
    int64_t nextPts_ = 0;
    bool initialized_ = false;
    bool eofReached_ = false;
};

CoreAudioDecoder::CoreAudioDecoder() : impl_(std::make_unique<Impl>()) {}
CoreAudioDecoder::~CoreAudioDecoder() = default;
CoreAudioDecoder::CoreAudioDecoder(CoreAudioDecoder&& other) noexcept = default;
CoreAudioDecoder& CoreAudioDecoder::operator=(CoreAudioDecoder&& other) noexcept = default;

CoreStatus CoreAudioDecoder::initialize(const StreamInfo& stream, int32_t targetSampleRate) {
    if (!impl_) {
        return CoreStatus::NotInitialized;
    }
    return impl_->initialize(stream, targetSampleRate);
}

CoreStatus CoreAudioDecoder::sendPacket(const EncodedPacket& packet) {
    if (!impl_) {
        return CoreStatus::NotInitialized;
    }
    return impl_->sendPacket(packet);
}

CoreStatus CoreAudioDecoder::decodeAudioBatch(int32_t maxSamples, RawAudioBatch& outBatch) {
    if (!impl_) {
        return CoreStatus::NotInitialized;
    }
    return impl_->decodeAudioBatch(maxSamples, outBatch);
}

CoreStatus CoreAudioDecoder::flush() {
    if (!impl_) {
        return CoreStatus::NotInitialized;
    }
    return impl_->flush();
}

void CoreAudioDecoder::reset() {
    if (!impl_) {
        return;
    }
    impl_->reset();
}

} // namespace openmedia
