/// @file test_stream_tracker.cpp
/// @brief Standalone C++17 unit test (TC-2.1.4) for StreamTracker multi-track MKV iteration and 64-track cap.

#include "importer/stream_tracker.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using openmedia::CodecId;
using openmedia::MediaType;
using openmedia::RationalTime;
using openmedia::StreamInfo;
using openmedia::importer::ImFileInfoRec8Ext;
using openmedia::importer::StreamTracker;
using openmedia::importer::kImBadStreamIndex;
using openmedia::importer::kImIterateStreams;
using openmedia::importer::kMaxSupportedStreams;
using openmedia::importer::kPrFalseVal;
using openmedia::importer::kPrTrueVal;
using openmedia::importer::kRandomAccessImportFlag;

namespace {

StreamInfo makeVideoStream(int32_t index) {
    StreamInfo s;
    s.index = index;
    s.type = MediaType::Video;
    s.codec = CodecId::AV1;
    s.timeBase = RationalTime{1, 1000};
    s.frameRate = RationalTime{30000, 1001};
    s.durationPts = 10010;
    s.width = 1920;
    s.height = 1080;
    return s;
}

StreamInfo makeAudioStream(int32_t index, int32_t channels) {
    StreamInfo s;
    s.index = index;
    s.type = MediaType::Audio;
    s.codec = CodecId::Opus;
    s.timeBase = RationalTime{1, 1000};
    s.durationPts = 10000;
    s.sampleRate = 48000;
    s.channels = channels;
    return s;
}

void testFiveStreamMkvFixture() {
    std::vector<StreamInfo> streams;
    streams.push_back(makeVideoStream(0));
    streams.push_back(makeAudioStream(1, 2));
    streams.push_back(makeAudioStream(2, 2));
    streams.push_back(makeAudioStream(3, 6));
    streams.push_back(makeAudioStream(4, 2));

    ImFileInfoRec8Ext info0;
    assert(StreamTracker::advanceStreamIteration(0, streams, info0) == kImIterateStreams);
    assert(info0.streamIdx == 0);
    assert(info0.hasVideo == kPrTrueVal);
    assert(info0.hasAudio == kPrFalseVal);
    assert(info0.vidInfo.imageWidth == 1920);
    assert(info0.vidInfo.imageHeight == 1080);
    assert(info0.vidScale == 30000);
    assert(info0.vidSampleSize == 1001);
    assert(info0.vidDuration == 300U);
    assert((info0.accessModes & kRandomAccessImportFlag) != 0U);

    for (int32_t idx = 1; idx <= 3; ++idx) {
        ImFileInfoRec8Ext info;
        assert(StreamTracker::advanceStreamIteration(idx, streams, info) == kImIterateStreams);
        assert(info.streamIdx == idx);
        assert(info.hasVideo == kPrFalseVal);
        assert(info.hasAudio == kPrTrueVal);
        assert(info.audSampleRate == 48000.0);
        assert(info.audNumChannels == streams[static_cast<size_t>(idx)].channels);
        assert(info.audDuration == 480000ULL);
    }

    ImFileInfoRec8Ext info4;
    assert(StreamTracker::advanceStreamIteration(4, streams, info4) == kImBadStreamIndex);
    assert(StreamTracker::advanceStreamIteration(5, streams, info4) == kImBadStreamIndex);
    assert(StreamTracker::advanceStreamIteration(-1, streams, info4) == kImBadStreamIndex);
}

void testMaxSupportedStreamsCap() {
    static_assert(kMaxSupportedStreams == 64, "kMaxSupportedStreams must be 64");
    std::vector<StreamInfo> streams;
    streams.reserve(100);
    streams.push_back(makeVideoStream(0));
    for (int32_t i = 1; i < 100; ++i) {
        streams.push_back(makeAudioStream(i, 2));
    }

    for (int32_t idx = 0; idx < kMaxSupportedStreams - 1; ++idx) {
        ImFileInfoRec8Ext info;
        assert(StreamTracker::advanceStreamIteration(idx, streams, info) == kImIterateStreams);
    }

    ImFileInfoRec8Ext cappedInfo;
    assert(StreamTracker::advanceStreamIteration(kMaxSupportedStreams - 1, streams, cappedInfo) == kImBadStreamIndex);
    assert(StreamTracker::advanceStreamIteration(kMaxSupportedStreams, streams, cappedInfo) == kImBadStreamIndex);

    std::vector<StreamInfo> emptyStreams;
    assert(StreamTracker::advanceStreamIteration(0, emptyStreams, cappedInfo) == kImBadStreamIndex);
}

} // namespace

int main() {
    testFiveStreamMkvFixture();
    testMaxSupportedStreamsCap();
    std::cout << "[PASS] TC-2.1.4 StreamTracker multi-track MKV iteration and 64-track cap\n";
    return 0;
}
