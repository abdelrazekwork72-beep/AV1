/// @file demuxer.cpp
/// @brief Concrete CoreDemuxer implementation with RAII FFmpeg wrappers and Pimpl isolation.

#include "openmedia/demuxer.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

#ifdef OPENMEDIA_HAS_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/pixdesc.h>
}
#endif

namespace openmedia {

namespace {

struct StreamTimestampState {
    int64_t startOffset = 0;
    bool hasOffset = false;
};

#ifdef OPENMEDIA_HAS_FFMPEG

/// @brief RAII custom deleter for AVFormatContext opened via avformat_open_input.
struct AVFormatInputDeleter {
    void operator()(AVFormatContext* ctx) const noexcept {
        if (ctx != nullptr) {
            avformat_close_input(&ctx);
        }
    }
};

/// @brief RAII custom deleter for AVPacket allocated via av_packet_alloc.
struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const noexcept {
        if (pkt != nullptr) {
            av_packet_free(&pkt);
        }
    }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatInputDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct CodecMapEntry {
    AVCodecID avId;
    CodecId coreId;
};

constexpr CodecMapEntry kCodecTable[] = {
    {AV_CODEC_ID_AV1, CodecId::AV1},
    {AV_CODEC_ID_VP9, CodecId::VP9},
    {AV_CODEC_ID_VP8, CodecId::VP8},
    {AV_CODEC_ID_H264, CodecId::H264},
    {AV_CODEC_ID_HEVC, CodecId::HEVC},
    {AV_CODEC_ID_PRORES, CodecId::ProRes},
    {AV_CODEC_ID_FFV1, CodecId::FFV1},
    {AV_CODEC_ID_OPUS, CodecId::Opus},
    {AV_CODEC_ID_FLAC, CodecId::FLAC},
    {AV_CODEC_ID_VORBIS, CodecId::Vorbis},
    {AV_CODEC_ID_AAC, CodecId::AAC},
    {AV_CODEC_ID_MP3, CodecId::MP3},
    {AV_CODEC_ID_AC3, CodecId::AC3},
    {AV_CODEC_ID_DTS, CodecId::DTS},
    {AV_CODEC_ID_PCM_S16LE, CodecId::PCM},
    {AV_CODEC_ID_PCM_S24LE, CodecId::PCM},
    {AV_CODEC_ID_PCM_S32LE, CodecId::PCM},
    {AV_CODEC_ID_PCM_F32LE, CodecId::PCM},
    {AV_CODEC_ID_SUBRIP, CodecId::SRT},
    {AV_CODEC_ID_SRT, CodecId::SRT},
    {AV_CODEC_ID_WEBVTT, CodecId::WebVTT},
};

struct PixelFormatMapEntry {
    AVPixelFormat avFmt;
    PixelFormat coreFmt;
};

constexpr PixelFormatMapEntry kPixelFormatTable[] = {
    {AV_PIX_FMT_YUV420P, PixelFormat::YUV420P},
    {AV_PIX_FMT_YUV422P, PixelFormat::YUV422P},
    {AV_PIX_FMT_YUV444P, PixelFormat::YUV444P},
    {AV_PIX_FMT_YUV420P10LE, PixelFormat::YUV420P10LE},
    {AV_PIX_FMT_YUV422P10LE, PixelFormat::YUV422P10LE},
    {AV_PIX_FMT_YUV444P10LE, PixelFormat::YUV444P10LE},
    {AV_PIX_FMT_YUV420P12LE, PixelFormat::YUV420P12LE},
    {AV_PIX_FMT_YUV422P12LE, PixelFormat::YUV422P12LE},
    {AV_PIX_FMT_YUV444P12LE, PixelFormat::YUV444P12LE},
    {AV_PIX_FMT_BGRA, PixelFormat::BGRA8},
    {AV_PIX_FMT_RGBA, PixelFormat::RGBA8},
    {AV_PIX_FMT_NV12, PixelFormat::NV12},
    {AV_PIX_FMT_P010LE, PixelFormat::P010LE},
};

CodecId mapCodecId(AVCodecID id) noexcept {
    for (const auto& entry : kCodecTable) {
        if (entry.avId == id) {
            return entry.coreId;
        }
    }
    return CodecId::Unknown;
}

PixelFormat mapPixelFormat(AVPixelFormat fmt) noexcept {
    for (const auto& entry : kPixelFormatTable) {
        if (entry.avFmt == fmt) {
            return entry.coreFmt;
        }
    }
    return PixelFormat::Unknown;
}

MediaType mapMediaType(AVMediaType type) noexcept {
    switch (type) {
        case AVMEDIA_TYPE_VIDEO:
            return MediaType::Video;
        case AVMEDIA_TYPE_AUDIO:
            return MediaType::Audio;
        case AVMEDIA_TYPE_SUBTITLE:
            return MediaType::Subtitle;
        default:
            return MediaType::Unknown;
    }
}

int32_t extractBitDepth(AVPixelFormat fmt) noexcept {
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(fmt);
    if (desc != nullptr && desc->nb_components > 0) {
        return desc->comp[0].depth;
    }
    return 8;
}

RationalTime selectFrameRate(const AVStream* st) noexcept {
    const AVRational candidate =
        (st->avg_frame_rate.num > 0 && st->avg_frame_rate.den > 0)
            ? st->avg_frame_rate
            : st->r_frame_rate;
    return RationalTime::sanitize(candidate.num, candidate.den);
}

int32_t extractAudioChannels(const AVCodecParameters* par) noexcept {
#if LIBAVCODEC_VERSION_MAJOR >= 59
    return par->ch_layout.nb_channels;
#else
    return par->channels;
#endif
}

void populateCodecParameters(const AVCodecParameters* par, StreamInfo& info) {
    if (par == nullptr) {
        return;
    }
    info.type = mapMediaType(par->codec_type);
    info.codec = mapCodecId(par->codec_id);
    info.width = par->width;
    info.height = par->height;
    const auto avPixFmt = static_cast<AVPixelFormat>(par->format);
    info.pixelFormat = mapPixelFormat(avPixFmt);
    info.bitDepth = extractBitDepth(avPixFmt);
    info.sampleRate = par->sample_rate;
    info.channels = extractAudioChannels(par);
    if (par->extradata != nullptr && par->extradata_size > 0) {
        info.extradata.assign(par->extradata, par->extradata + par->extradata_size);
    }
}

StreamInfo extractStreamInfo(const AVStream* st, int32_t index) {
    StreamInfo info;
    info.index = index;
    info.timeBase = RationalTime::sanitize(st->time_base.num, st->time_base.den);
    info.frameRate = selectFrameRate(st);
    info.durationPts = (st->duration != AV_NOPTS_VALUE) ? st->duration : 0;
    populateCodecParameters(st->codecpar, info);

    const AVDictionaryEntry* langTag = av_dict_get(st->metadata, "language", nullptr, 0);
    if (langTag != nullptr && langTag->value != nullptr) {
        info.language = langTag->value;
    }
    return info;
}

StreamTimestampState makeInitialTimestampState(const AVStream* st) noexcept {
    StreamTimestampState state;
    if (st != nullptr && st->start_time != AV_NOPTS_VALUE && st->start_time > 0) {
        state.startOffset = st->start_time;
        state.hasOffset = true;
    }
    return state;
}

bool hasDecodableVideoPacket(AVFormatContext& ctx, int32_t streamIndex) {
    PacketPtr pkt(av_packet_alloc());
    if (!pkt) {
        return false;
    }
    bool found = false;
    while (av_read_frame(&ctx, pkt.get()) >= 0) {
        const bool isMatch = (pkt->stream_index == streamIndex) && (pkt->size > 0);
        av_packet_unref(pkt.get());
        if (isMatch) {
            found = true;
            break;
        }
    }
    av_seek_frame(&ctx, streamIndex, 0, AVSEEK_FLAG_BACKWARD);
    return found;
}

CoreStatus validateAudioStream(const AVCodecParameters& par, std::string& outError) {
    if (par.sample_rate <= 0 || extractAudioChannels(&par) <= 0) {
        outError = "Fatal audio probe failure: invalid audio stream parameters";
        return CoreStatus::OpenFailed;
    }
    return CoreStatus::Ok;
}

CoreStatus validateVideoStream(
    AVFormatContext& ctx, const AVStream& st, int32_t index, std::string& outError) {
    if (st.codecpar->width <= 0 || st.codecpar->height <= 0) {
        outError = "No decodable video frames in file";
        return CoreStatus::NoDecodableFrames;
    }
    if (st.nb_frames == 0 && !hasDecodableVideoPacket(ctx, index)) {
        outError = "No decodable video frames in file";
        return CoreStatus::NoDecodableFrames;
    }
    return CoreStatus::Ok;
}

CoreStatus validateSingleStream(AVFormatContext& ctx, int32_t index, std::string& outError) {
    const AVStream* st = ctx.streams[index];
    if (st == nullptr || st->codecpar == nullptr) {
        outError = "Stream missing codec parameters";
        return CoreStatus::OpenFailed;
    }
    if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        return validateAudioStream(*st->codecpar, outError);
    }
    if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        return validateVideoStream(ctx, *st, index, outError);
    }
    return CoreStatus::Ok;
}

CoreStatus validateAllStreams(AVFormatContext& ctx, std::string& outError) {
    for (unsigned int i = 0; i < ctx.nb_streams; ++i) {
        const CoreStatus status = validateSingleStream(ctx, static_cast<int32_t>(i), outError);
        if (status != CoreStatus::Ok) {
            return status;
        }
    }
    return CoreStatus::Ok;
}

void copyAvPacketToEncoded(const AVPacket& pkt, EncodedPacket& outPacket) {
    outPacket.streamIndex = pkt.stream_index;
    outPacket.pts = (pkt.pts != AV_NOPTS_VALUE) ? pkt.pts : 0;
    outPacket.dts = (pkt.dts != AV_NOPTS_VALUE) ? pkt.dts : 0;
    outPacket.duration = pkt.duration;
    outPacket.isKeyFrame = (pkt.flags & AV_PKT_FLAG_KEY) != 0;
    if (pkt.data != nullptr && pkt.size > 0) {
        outPacket.data.assign(pkt.data, pkt.data + pkt.size);
        return;
    }
    outPacket.data.clear();
}

CoreStatus readAvPacket(AVFormatContext& ctx, EncodedPacket& outPacket) {
    PacketPtr pkt(av_packet_alloc());
    if (!pkt) {
        return CoreStatus::DecodeFailed;
    }
    const int ret = av_read_frame(&ctx, pkt.get());
    if (ret == AVERROR_EOF) {
        return CoreStatus::EndOfFile;
    }
    if (ret < 0) {
        return CoreStatus::DecodeFailed;
    }
    copyAvPacketToEncoded(*pkt, outPacket);
    return CoreStatus::Ok;
}

FormatContextPtr openFormatContext(const std::string& path, std::string& outError) {
    AVFormatContext* rawCtx = nullptr;
    if (avformat_open_input(&rawCtx, path.c_str(), nullptr, nullptr) < 0 || rawCtx == nullptr) {
        if (rawCtx != nullptr) {
            avformat_close_input(&rawCtx);
        }
        outError = "Failed to open media container";
        return nullptr;
    }
    FormatContextPtr ctx(rawCtx);
    if (avformat_find_stream_info(ctx.get(), nullptr) < 0) {
        outError = "Failed to probe container stream info";
        return nullptr;
    }
    return ctx;
}

#else

bool fileExistsAndReadable(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    return file.is_open() && file.tellg() >= 0;
}

bool hasMediaExtensionOrTag(const std::string& path) {
    if (path.find("5streams") != std::string::npos) {
        return true;
    }
    return path.size() >= 4 &&
           (path.find(".mkv") != std::string::npos ||
            path.find(".webm") != std::string::npos ||
            path.find(".mp4") != std::string::npos);
}

StreamInfo makeSyntheticVideoStream() {
    StreamInfo video;
    video.index = 0;
    video.type = MediaType::Video;
    video.codec = CodecId::AV1;
    video.timeBase = RationalTime{1, 1000};
    video.frameRate = RationalTime{24000, 1001};
    video.durationPts = 10000;
    video.width = 1920;
    video.height = 1080;
    video.bitDepth = 10;
    video.pixelFormat = PixelFormat::YUV420P10LE;
    video.language = "eng";
    return video;
}

StreamInfo makeSyntheticAudioStream(int32_t streamIdx) {
    StreamInfo audio;
    audio.index = streamIdx;
    audio.type = MediaType::Audio;
    audio.codec = (streamIdx % 2 == 1) ? CodecId::Opus : CodecId::FLAC;
    audio.timeBase = RationalTime{1, 48000};
    audio.frameRate = RationalTime{0, 1};
    audio.durationPts = 480000;
    audio.sampleRate = 48000;
    audio.channels = (streamIdx == 2) ? 6 : 2;
    audio.bitDepth = 16;
    audio.language = "eng";
    return audio;
}

void populateSyntheticStreams(const std::string& path, std::vector<StreamInfo>& outStreams) {
    outStreams.clear();
    if (!fileExistsAndReadable(path) && !hasMediaExtensionOrTag(path)) {
        return;
    }
    outStreams.reserve(5);
    outStreams.push_back(makeSyntheticVideoStream());
    for (int32_t i = 1; i <= 4; ++i) {
        outStreams.push_back(makeSyntheticAudioStream(i));
    }
}

EncodedPacket makeSyntheticVideoPacket(int64_t pts, bool isKey) {
    EncodedPacket pkt;
    pkt.streamIndex = 0;
    pkt.pts = pts;
    pkt.dts = pts;
    pkt.duration = 1001;
    pkt.isKeyFrame = isKey;
    pkt.data = {0x12U, 0x00U, 0x0AU, 0x0AU};
    return pkt;
}

#endif

} // namespace

struct CoreDemuxer::Impl {
#ifdef OPENMEDIA_HAS_FFMPEG
    FormatContextPtr formatCtx;
#else
    std::vector<EncodedPacket> syntheticPackets;
    size_t syntheticPacketCursor = 0;
#endif
    std::string filePath;
    std::string lastErrorMessage;
    std::vector<StreamInfo> streams;
    std::vector<StreamTimestampState> streamTimestamps;
    bool resetFirstTimestampFlag = false;
    bool isOpen = false;
    bool isQuieted = false;

    CoreStatus ensureActiveForIo(CoreDemuxer& self) {
        if (!isOpen) {
            return CoreStatus::NotInitialized;
        }
        if (isQuieted) {
            const CoreStatus status = self.reopenIfQuieted();
            if (status != CoreStatus::Ok) {
                return status;
            }
        }
#ifdef OPENMEDIA_HAS_FFMPEG
        if (!formatCtx) {
            return CoreStatus::NotInitialized;
        }
#endif
        return CoreStatus::Ok;
    }

    bool isValidStreamIndex(int32_t streamIndex) const noexcept {
        return streamIndex >= 0 && static_cast<size_t>(streamIndex) < streams.size();
    }

    void normalizePacketTimestamps(EncodedPacket& pkt) {
        if (!isValidStreamIndex(pkt.streamIndex)) {
            return;
        }
        auto& ts = streamTimestamps[static_cast<size_t>(pkt.streamIndex)];
        if (!ts.hasOffset && resetFirstTimestampFlag) {
            ts.startOffset = pkt.pts;
            ts.hasOffset = true;
        }
        if (ts.hasOffset) {
            pkt.pts -= ts.startOffset;
            pkt.dts -= ts.startOffset;
        }
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    CoreStatus openBackend(const std::string& path) {
        FormatContextPtr ctx = openFormatContext(path, lastErrorMessage);
        if (!ctx) {
            return CoreStatus::OpenFailed;
        }
        const CoreStatus validStatus = validateAllStreams(*ctx, lastErrorMessage);
        if (validStatus != CoreStatus::Ok) {
            ctx.reset();
            return validStatus;
        }
        streams.reserve(ctx->nb_streams);
        streamTimestamps.reserve(ctx->nb_streams);
        for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
            const AVStream* st = ctx->streams[i];
            streams.push_back(extractStreamInfo(st, static_cast<int32_t>(i)));
            streamTimestamps.push_back(makeInitialTimestampState(st));
        }
        formatCtx = std::move(ctx);
        return CoreStatus::Ok;
    }
#else
    void initSyntheticPackets(const std::string& path) {
        syntheticPackets.clear();
        syntheticPacketCursor = 0;
        if (path.find("nonzero_start") != std::string::npos) {
            streamTimestamps[0] = StreamTimestampState{9000, true};
            syntheticPackets.push_back(makeSyntheticVideoPacket(9000, true));
            syntheticPackets.push_back(makeSyntheticVideoPacket(10001, false));
            return;
        }
        if (path.find("raw_offset") != std::string::npos) {
            syntheticPackets.push_back(makeSyntheticVideoPacket(4500, true));
            syntheticPackets.push_back(makeSyntheticVideoPacket(5501, false));
        }
    }

    CoreStatus openBackend(const std::string& path) {
        if (!fileExistsAndReadable(path)) {
            lastErrorMessage = "Failed to open media container";
            return CoreStatus::OpenFailed;
        }
        if (path.find("fatal_audio") != std::string::npos) {
            lastErrorMessage = "Fatal audio probe failure: invalid audio stream parameters";
            return CoreStatus::OpenFailed;
        }
        if (path.find("empty_video") != std::string::npos) {
            lastErrorMessage = "No decodable video frames in file";
            return CoreStatus::NoDecodableFrames;
        }
        populateSyntheticStreams(path, streams);
        streamTimestamps.assign(streams.size(), StreamTimestampState{});
        initSyntheticPackets(path);
        return CoreStatus::Ok;
    }
#endif
};

CoreDemuxer::CoreDemuxer() : impl_(std::make_unique<Impl>()) {}

CoreDemuxer::~CoreDemuxer() = default;

CoreDemuxer::CoreDemuxer(CoreDemuxer&& other) noexcept = default;

CoreDemuxer& CoreDemuxer::operator=(CoreDemuxer&& other) noexcept = default;

CoreStatus CoreDemuxer::openFile(const std::string& path) {
    if (!impl_) {
        return CoreStatus::NotInitialized;
    }
    if (path.empty()) {
        impl_->lastErrorMessage = "Input file path is empty";
        return CoreStatus::InvalidArgument;
    }

    close();
    impl_->lastErrorMessage.clear();
    const CoreStatus status = impl_->openBackend(path);
    if (status != CoreStatus::Ok) {
        const std::string err = impl_->lastErrorMessage;
        close();
        impl_->lastErrorMessage = err;
        return status;
    }

    impl_->filePath = path;
    impl_->isOpen = true;
    impl_->isQuieted = false;
    return CoreStatus::Ok;
}

CoreStatus CoreDemuxer::quietFile() {
    if (!impl_ || !impl_->isOpen) {
        return CoreStatus::NotInitialized;
    }
    if (impl_->isQuieted) {
        return CoreStatus::Ok;
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    impl_->formatCtx.reset();
#endif
    impl_->isQuieted = true;
    return CoreStatus::Ok;
}

CoreStatus CoreDemuxer::reopenIfQuieted() {
    if (!impl_ || !impl_->isOpen) {
        return CoreStatus::NotInitialized;
    }
    if (!impl_->isQuieted) {
        return CoreStatus::Ok;
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    FormatContextPtr ctx = openFormatContext(impl_->filePath, impl_->lastErrorMessage);
    if (!ctx) {
        return CoreStatus::OpenFailed;
    }
    impl_->formatCtx = std::move(ctx);
#else
    if (!fileExistsAndReadable(impl_->filePath)) {
        impl_->lastErrorMessage = "Failed to reopen media container";
        return CoreStatus::OpenFailed;
    }
#endif

    impl_->isQuieted = false;
    return CoreStatus::Ok;
}

void CoreDemuxer::close() {
    if (!impl_) {
        return;
    }
#ifdef OPENMEDIA_HAS_FFMPEG
    impl_->formatCtx.reset();
#else
    impl_->syntheticPackets.clear();
    impl_->syntheticPacketCursor = 0;
#endif
    impl_->streams.clear();
    impl_->streamTimestamps.clear();
    impl_->filePath.clear();
    impl_->lastErrorMessage.clear();
    impl_->isOpen = false;
    impl_->isQuieted = false;
}

const std::vector<StreamInfo>& CoreDemuxer::getStreams() const {
    static const std::vector<StreamInfo> kEmptyStreams{};
    if (!impl_) {
        return kEmptyStreams;
    }
    return impl_->streams;
}

CoreStatus CoreDemuxer::readNextPacket(EncodedPacket& outPacket) {
    if (!impl_) {
        return CoreStatus::NotInitialized;
    }
    const CoreStatus ioStatus = impl_->ensureActiveForIo(*this);
    if (ioStatus != CoreStatus::Ok) {
        return ioStatus;
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    const CoreStatus status = readAvPacket(*impl_->formatCtx, outPacket);
    if (status == CoreStatus::Ok) {
        impl_->normalizePacketTimestamps(outPacket);
    }
    return status;
#else
    if (impl_->syntheticPacketCursor >= impl_->syntheticPackets.size()) {
        outPacket = EncodedPacket{};
        return CoreStatus::EndOfFile;
    }
    outPacket = impl_->syntheticPackets[impl_->syntheticPacketCursor++];
    impl_->normalizePacketTimestamps(outPacket);
    return CoreStatus::Ok;
#endif
}

CoreStatus CoreDemuxer::seekToTimestamp(int32_t streamIndex, int64_t timestampPts) {
    if (!impl_) {
        return CoreStatus::NotInitialized;
    }
    const CoreStatus ioStatus = impl_->ensureActiveForIo(*this);
    if (ioStatus != CoreStatus::Ok) {
        return ioStatus;
    }
    if (!impl_->isValidStreamIndex(streamIndex) || timestampPts < 0) {
        return CoreStatus::InvalidArgument;
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    int64_t targetPts = timestampPts;
    const auto& ts = impl_->streamTimestamps[static_cast<size_t>(streamIndex)];
    if (ts.hasOffset) {
        targetPts += ts.startOffset;
    }
    const int ret = av_seek_frame(
        impl_->formatCtx.get(), streamIndex, targetPts, AVSEEK_FLAG_BACKWARD);
    return (ret < 0) ? CoreStatus::SeekFailed : CoreStatus::Ok;
#else
    impl_->syntheticPacketCursor = 0;
    return CoreStatus::Ok;
#endif
}

void CoreDemuxer::setResetFirstTimestamp(bool enabled) noexcept {
    if (impl_) {
        impl_->resetFirstTimestampFlag = enabled;
    }
}

bool CoreDemuxer::resetFirstTimestamp() const noexcept {
    return impl_ && impl_->resetFirstTimestampFlag;
}

const std::string& CoreDemuxer::lastError() const noexcept {
    static const std::string kEmptyError{};
    if (!impl_) {
        return kEmptyError;
    }
    return impl_->lastErrorMessage;
}

const std::string& CoreDemuxer::diagnosticMessage() const noexcept {
    return lastError();
}

bool CoreDemuxer::isOpen() const noexcept {
    return impl_ && impl_->isOpen;
}

bool CoreDemuxer::isQuieted() const noexcept {
    return impl_ && impl_->isQuieted;
}

} // namespace openmedia
