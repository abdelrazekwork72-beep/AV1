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

StreamInfo extractStreamInfo(const AVStream* st, int32_t index) {
    StreamInfo info;
    info.index = index;
    info.timeBase = RationalTime::sanitize(st->time_base.num, st->time_base.den);
    info.frameRate = selectFrameRate(st);
    info.durationPts = (st->duration != AV_NOPTS_VALUE) ? st->duration : 0;

    const AVCodecParameters* par = st->codecpar;
    if (par != nullptr) {
        info.type = mapMediaType(par->codec_type);
        info.codec = mapCodecId(par->codec_id);
        info.width = par->width;
        info.height = par->height;
        const auto avPixFmt = static_cast<AVPixelFormat>(par->format);
        info.pixelFormat = mapPixelFormat(avPixFmt);
        info.bitDepth = extractBitDepth(avPixFmt);
        info.sampleRate = par->sample_rate;
#if LIBAVCODEC_VERSION_MAJOR >= 59
        info.channels = par->ch_layout.nb_channels;
#else
        info.channels = par->channels;
#endif
        if (par->extradata != nullptr && par->extradata_size > 0) {
            info.extradata.assign(par->extradata, par->extradata + par->extradata_size);
        }
    }

    const AVDictionaryEntry* langTag = av_dict_get(st->metadata, "language", nullptr, 0);
    if (langTag != nullptr && langTag->value != nullptr) {
        info.language = langTag->value;
    }
    return info;
}

FormatContextPtr openFormatContext(const std::string& path) {
    AVFormatContext* rawCtx = nullptr;
    if (avformat_open_input(&rawCtx, path.c_str(), nullptr, nullptr) < 0 || rawCtx == nullptr) {
        return nullptr;
    }
    FormatContextPtr ctx(rawCtx);
    if (avformat_find_stream_info(ctx.get(), nullptr) < 0) {
        return nullptr;
    }
    return ctx;
}

#else

bool fileExistsAndReadable(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return file.is_open();
}

#endif

} // namespace

struct CoreDemuxer::Impl {
#ifdef OPENMEDIA_HAS_FFMPEG
    FormatContextPtr formatCtx;
#endif
    std::string filePath;
    std::vector<StreamInfo> streams;
    bool isOpen = false;
    bool isQuieted = false;
};

CoreDemuxer::CoreDemuxer() : impl_(std::make_unique<Impl>()) {}

CoreDemuxer::~CoreDemuxer() = default;

CoreDemuxer::CoreDemuxer(CoreDemuxer&& other) noexcept = default;

CoreDemuxer& CoreDemuxer::operator=(CoreDemuxer&& other) noexcept = default;

CoreStatus CoreDemuxer::openFile(const std::string& path) {
    if (path.empty()) {
        return CoreStatus::InvalidArgument;
    }

    close();

#ifdef OPENMEDIA_HAS_FFMPEG
    FormatContextPtr ctx = openFormatContext(path);
    if (!ctx) {
        return CoreStatus::OpenFailed;
    }

    impl_->streams.reserve(ctx->nb_streams);
    for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
        if (ctx->streams[i] != nullptr) {
            impl_->streams.push_back(extractStreamInfo(ctx->streams[i], static_cast<int32_t>(i)));
        }
    }
    impl_->formatCtx = std::move(ctx);
#else
    if (!fileExistsAndReadable(path)) {
        return CoreStatus::OpenFailed;
    }
#endif

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
    FormatContextPtr ctx = openFormatContext(impl_->filePath);
    if (!ctx) {
        return CoreStatus::OpenFailed;
    }
    impl_->formatCtx = std::move(ctx);
#else
    if (!fileExistsAndReadable(impl_->filePath)) {
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
#endif
    impl_->streams.clear();
    impl_->filePath.clear();
    impl_->isOpen = false;
    impl_->isQuieted = false;
}

const std::vector<StreamInfo>& CoreDemuxer::getStreams() const {
    return impl_->streams;
}

CoreStatus CoreDemuxer::readNextPacket(EncodedPacket& outPacket) {
    if (!impl_ || !impl_->isOpen) {
        return CoreStatus::NotInitialized;
    }
    if (impl_->isQuieted) {
        const CoreStatus reopenStatus = reopenIfQuieted();
        if (reopenStatus != CoreStatus::Ok) {
            return reopenStatus;
        }
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    if (!impl_->formatCtx) {
        return CoreStatus::NotInitialized;
    }
    PacketPtr pkt(av_packet_alloc());
    if (!pkt) {
        return CoreStatus::DecodeFailed;
    }
    const int ret = av_read_frame(impl_->formatCtx.get(), pkt.get());
    if (ret == AVERROR_EOF) {
        return CoreStatus::EndOfFile;
    }
    if (ret < 0) {
        return CoreStatus::DecodeFailed;
    }

    outPacket.streamIndex = pkt->stream_index;
    outPacket.pts = (pkt->pts != AV_NOPTS_VALUE) ? pkt->pts : 0;
    outPacket.dts = (pkt->dts != AV_NOPTS_VALUE) ? pkt->dts : 0;
    outPacket.duration = pkt->duration;
    outPacket.isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
    if (pkt->data != nullptr && pkt->size > 0) {
        outPacket.data.assign(pkt->data, pkt->data + pkt->size);
    } else {
        outPacket.data.clear();
    }
    return CoreStatus::Ok;
#else
    outPacket = EncodedPacket{};
    return CoreStatus::EndOfFile;
#endif
}

CoreStatus CoreDemuxer::seekToTimestamp(int32_t streamIndex, int64_t timestampPts) {
    if (!impl_ || !impl_->isOpen) {
        return CoreStatus::NotInitialized;
    }
    if (streamIndex < 0 || static_cast<size_t>(streamIndex) >= impl_->streams.size()) {
        return CoreStatus::InvalidArgument;
    }
    if (impl_->isQuieted) {
        const CoreStatus reopenStatus = reopenIfQuieted();
        if (reopenStatus != CoreStatus::Ok) {
            return reopenStatus;
        }
    }

#ifdef OPENMEDIA_HAS_FFMPEG
    if (!impl_->formatCtx) {
        return CoreStatus::NotInitialized;
    }
    const int ret = av_seek_frame(
        impl_->formatCtx.get(), streamIndex, timestampPts, AVSEEK_FLAG_BACKWARD);
    return (ret < 0) ? CoreStatus::SeekFailed : CoreStatus::Ok;
#else
    (void)timestampPts;
    return CoreStatus::Ok;
#endif
}

bool CoreDemuxer::isOpen() const noexcept {
    return impl_ && impl_->isOpen;
}

bool CoreDemuxer::isQuieted() const noexcept {
    return impl_ && impl_->isQuieted;
}

} // namespace openmedia
