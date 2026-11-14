/// @file test_file_probe_timebase.cpp
/// @brief Standalone C++17 unit tests (TC-2.1.3) for FileProbe.

#include "importer/file_probe.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

using openmedia::CodecId;
using openmedia::CoreStatus;
using openmedia::EncodedPacket;
using openmedia::ICoreDemuxer;
using openmedia::MediaType;
using openmedia::RationalTime;
using openmedia::StreamInfo;
using openmedia::importer::FileProbe;
using openmedia::importer::ImFileInfoRec8Ext;
using openmedia::importer::ImOpenFileRec8Ext;
using openmedia::importer::kImBadFile;
using openmedia::importer::kImNoErr;
using openmedia::importer::kPrFalseVal;
using openmedia::importer::kPrTrueVal;
using openmedia::importer::kRandomAccessImportFlag;

class MockDemuxer final : public ICoreDemuxer {
public:
    CoreStatus openStatus = CoreStatus::Ok;
    bool closed = false;
    std::string lastOpenedPath;
    std::vector<StreamInfo> streams;

    CoreStatus openFile(const std::string& path) override {
        lastOpenedPath = path;
        closed = false;
        return openStatus;
    }
    CoreStatus quietFile() override { return CoreStatus::Ok; }
    CoreStatus reopenIfQuieted() override { return CoreStatus::Ok; }
    void close() override { closed = true; }
    const std::vector<StreamInfo>& getStreams() const override { return streams; }
    CoreStatus readNextPacket(EncodedPacket&) override { return CoreStatus::EndOfFile; }
    CoreStatus seekToTimestamp(int32_t, int64_t) override { return CoreStatus::Ok; }
};

void testUtf16ArabicCjkAndSurrogates() {
    // Arabic path: C:\فيديو\ملف.mkv
    const std::u16string arabicUtf16 = u"C:\\\u0641\u064a\u062f\u064a\u0648\\\u0645\u0644\u0641.mkv";
    const std::string arabicUtf8 = FileProbe::utf16ToUtf8(arabicUtf16);
    assert(arabicUtf8 == "C:\\\xD9\x81\xD9\x8A\xD8\xAF\xD9\x8A\xD9\x88\\\xD9\x85\xD9\x84\xD9\x81.mkv");

    // CJK path: /媒体/動画.webm
    const std::u16string cjkUtf16 = u"/\u5a92\u4f53/\u52d5\u753b.webm";
    const std::string cjkUtf8 = FileProbe::utf16ToUtf8(cjkUtf16);
    assert(cjkUtf8 == "/\xE5\xAA\x92\xE4\xBD\x93/\xE5\x8B\x95\xE7\x94\xBB.webm");

    // Surrogate pair (U+1F3AC CLAPPER BOARD + U+20000 CJK Extension B)
    const std::u16string surrogateUtf16 = u"\U0001F3AC_\U00020000.av1";
    const std::string surrogateUtf8 = FileProbe::utf16ToUtf8(surrogateUtf16);
    assert(surrogateUtf8 == "\xF0\x9F\x8E\xAC_\xF0\xA0\x80\x80.av1");

    // Empty and unpaired surrogate safety
    assert(FileProbe::utf16ToUtf8(u"").empty());
    const std::u16string badSurrogate{static_cast<char16_t>(0xD800U), u'a'};
    assert(FileProbe::utf16ToUtf8(badSurrogate) == "\xEF\xBF\xBD" "a");
}

void testNativeDelegationOnStandardMp4AndMov() {
    StreamInfo h264Video;
    h264Video.index = 0;
    h264Video.type = MediaType::Video;
    h264Video.codec = CodecId::H264;

    StreamInfo aacAudio;
    aacAudio.index = 1;
    aacAudio.type = MediaType::Audio;
    aacAudio.codec = CodecId::Unknown; // Standard AAC

    MockDemuxer demuxer;
    demuxer.streams = {h264Video, aacAudio};

    ImOpenFileRec8Ext mp4Rec;
    mp4Rec.fileinfo.filepath = u"C:\\clips\\standard_h264.MP4";
    assert(FileProbe::openAndProbe(mp4Rec, demuxer) == kImBadFile);
    assert(demuxer.closed);

    ImOpenFileRec8Ext movRec;
    movRec.fileinfo.filepath = u"/Volumes/Media/camera.mov";
    assert(FileProbe::openAndProbe(movRec, demuxer) == kImBadFile);
    assert(demuxer.closed);

    // Same H.264 stream inside MKV must NOT delegate to native
    ImOpenFileRec8Ext mkvRec;
    mkvRec.fileinfo.filepath = u"/Volumes/Media/h264_in_mkv.mkv";
    assert(FileProbe::openAndProbe(mkvRec, demuxer) == kImNoErr);
    assert(!demuxer.closed);

    // MP4 containing AV1 must NOT delegate to native
    demuxer.streams[0].codec = CodecId::AV1;
    assert(FileProbe::openAndProbe(mp4Rec, demuxer) == kImNoErr);
    assert(!demuxer.closed);

    // MP4 containing H.264 + Opus must NOT delegate to native
    demuxer.streams[0].codec = CodecId::H264;
    demuxer.streams[1].codec = CodecId::Opus;
    assert(FileProbe::openAndProbe(mp4Rec, demuxer) == kImNoErr);
}

void testPopulateFileInfoTimebaseAndFlags() {
    StreamInfo video;
    video.index = 0;
    video.type = MediaType::Video;
    video.codec = CodecId::AV1;
    video.width = 3840;
    video.height = 2160;
    video.frameRate = RationalTime{30000, 1001};
    video.durationPts = 90090;

    StreamInfo audio;
    audio.index = 1;
    audio.type = MediaType::Audio;
    audio.codec = CodecId::Opus;
    audio.sampleRate = 48000;
    audio.channels = 2;
    audio.durationPts = 144000;

    ImFileInfoRec8Ext info;
    assert(FileProbe::populateFileInfo({video, audio}, info) == kImNoErr);
    assert(info.hasVideo == kPrTrueVal);
    assert(info.hasAudio == kPrTrueVal);
    assert(info.vidInfo.imageWidth == 3840);
    assert(info.vidInfo.imageHeight == 2160);
    assert(info.vidScale == 30000);
    assert(info.vidSampleSize == 1001);
    assert(info.vidInfo.supportsGetSourceVideo == kPrTrueVal);
    assert(info.vidInfo.supportsAsyncIO == kPrTrueVal);
    assert((info.accessModes & kRandomAccessImportFlag) != 0U);
    assert(info.highMemUsage == kPrTrueVal);
    assert(info.audSampleRate == 48000.0);
    assert(info.audNumChannels == 2);

    // Zero / negative denominator guard (den <= 0 ? 1 : den)
    video.frameRate = RationalTime{24, 0};
    ImFileInfoRec8Ext zeroDenInfo;
    assert(FileProbe::populateFileInfo({video}, zeroDenInfo) == kImNoErr);
    assert(zeroDenInfo.vidScale == 24);
    assert(zeroDenInfo.vidSampleSize == 1);
    assert(zeroDenInfo.hasAudio == kPrFalseVal);

    video.frameRate = RationalTime{60, -5};
    ImFileInfoRec8Ext negDenInfo;
    assert(FileProbe::populateFileInfo({video}, negDenInfo) == kImNoErr);
    assert(negDenInfo.vidScale == 60);
    assert(negDenInfo.vidSampleSize == 1);

    // Empty streams returns kImBadFile
    ImFileInfoRec8Ext emptyInfo;
    assert(FileProbe::populateFileInfo({}, emptyInfo) == kImBadFile);
}

} // namespace

int main() {
    testUtf16ArabicCjkAndSurrogates();
    testNativeDelegationOnStandardMp4AndMov();
    testPopulateFileInfoTimebaseAndFlags();
    std::cout << "[PASS] TC-2.1.3 FileProbe UTF-16, native fallback, and rational timebase tests passed.\n";
    return 0;
}
