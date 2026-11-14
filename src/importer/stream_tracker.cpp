#include "importer/stream_tracker.hpp"

namespace openmedia::importer {
namespace {

double computeDurationSeconds(const StreamInfo& stream) noexcept {
    if (stream.durationPts <= 0) {
        return 0.0;
    }
    const RationalTime safeTb = RationalTime::sanitize(stream.timeBase.num, stream.timeBase.den);
    return (static_cast<double>(stream.durationPts) * static_cast<double>(safeTb.num)) /
           static_cast<double>(safeTb.den);
}

void populateVideoInfo(const StreamInfo& stream, ImFileInfoRec8Ext& outInfo) noexcept {
    outInfo.hasVideo = kPrTrueVal;
    outInfo.vidInfo.imageWidth = stream.width;
    outInfo.vidInfo.imageHeight = stream.height;
    outInfo.vidInfo.pixelAspectNum = 1U;
    outInfo.vidInfo.pixelAspectDen = 1U;
    outInfo.vidInfo.supportsGetSourceVideo = kPrTrueVal;
    outInfo.vidInfo.supportsAsyncIO = kPrTrueVal;
    outInfo.accessModes |= kRandomAccessImportFlag;

    const RationalTime safeRate = RationalTime::sanitize(stream.frameRate.num, stream.frameRate.den);
    const bool hasValidRate = safeRate.num > 0;
    outInfo.vidScale = hasValidRate ? static_cast<csSDK_int32>(safeRate.num) : 30;
    outInfo.vidSampleSize = hasValidRate ? static_cast<csSDK_int32>(safeRate.den) : 1;

    const double durationSec = computeDurationSeconds(stream);
    const double fps = static_cast<double>(outInfo.vidScale) / static_cast<double>(outInfo.vidSampleSize);
    outInfo.vidDuration = (durationSec > 0.0 && fps > 0.0)
        ? static_cast<csSDK_uint32>(durationSec * fps + 0.5)
        : 0U;
}

void populateAudioInfo(const StreamInfo& stream, ImFileInfoRec8Ext& outInfo) noexcept {
    outInfo.hasAudio = kPrTrueVal;
    outInfo.audSampleRate = (stream.sampleRate > 0) ? static_cast<double>(stream.sampleRate) : 48000.0;
    outInfo.audNumChannels = (stream.channels > 0) ? stream.channels : 2;

    const double durationSec = computeDurationSeconds(stream);
    outInfo.audDuration = (durationSec > 0.0)
        ? static_cast<csSDK_uint64>(durationSec * outInfo.audSampleRate + 0.5)
        : 0ULL;
}

void populateStreamFields(const StreamInfo& stream, ImFileInfoRec8Ext& outInfo) noexcept {
    if (stream.type == MediaType::Video) {
        populateVideoInfo(stream, outInfo);
        return;
    }
    if (stream.type == MediaType::Audio) {
        populateAudioInfo(stream, outInfo);
    }
}

} // namespace

csSDK_int32 StreamTracker::advanceStreamIteration(int32_t streamIdx,
                                                  const std::vector<StreamInfo>& streams,
                                                  ImFileInfoRec8Ext& outInfo) {
    const int32_t totalCount = static_cast<int32_t>(streams.size());
    const int32_t cappedCount = (totalCount > kMaxSupportedStreams) ? kMaxSupportedStreams : totalCount;
    if (streamIdx < 0 || streamIdx >= cappedCount - 1) {
        return kImBadStreamIndex;
    }

    outInfo.streamIdx = streamIdx;
    outInfo.hasVideo = kPrFalseVal;
    outInfo.hasAudio = kPrFalseVal;
    populateStreamFields(streams[static_cast<size_t>(streamIdx)], outInfo);
    return kImIterateStreams;
}

} // namespace openmedia::importer
