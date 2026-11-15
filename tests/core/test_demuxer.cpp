/// @file test_demuxer.cpp
/// @brief Standalone unit tests (TC-1.2.1) for CoreDemuxer.

#include "openmedia/demuxer.hpp"
#include "openmedia/muxer.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

const std::vector<uint8_t> kValidAv1Extradata = {
    0x0AU, 0x0AU, 0x00U, 0x00U, 0x00U, 0x02U,
    0xAFU, 0xFFU, 0x80U, 0x5FU, 0x00U, 0x08U
};

const std::vector<uint8_t> kValidAv1KeyPacket = {
    0x12U, 0x00U, 0x0AU, 0x0AU, 0x00U, 0x00U, 0x00U, 0x02U,
    0xAFU, 0xFFU, 0x89U, 0x5FU, 0x20U, 0x08U, 0x32U, 0x0CU,
    0x10U, 0x00U, 0xA8U, 0x82U, 0x04U, 0x10U, 0x20U, 0x81U,
    0x00U, 0x02U, 0x98U, 0x20U
};

void writeDummyFixtureFile(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    assert(out.is_open());
    const char kStub[] = "OPENMEDIA_FIXTURE";
    out.write(kStub, sizeof(kStub) - 1);
    out.close();
}

void testRationalTimebaseSanitization() {
    const auto zeroDen = openmedia::RationalTime::sanitize(1, 0);
    assert(zeroDen.num == 1);
    assert(zeroDen.den == 1);
    assert(std::abs(zeroDen.toSeconds() - 1.0) < 1e-9);

    const auto negDen = openmedia::RationalTime::sanitize(30000, -1001);
    assert(negDen.num == 30000);
    assert(negDen.den == 1);

    const auto valid = openmedia::RationalTime::sanitize(1, 90000);
    assert(valid.num == 1);
    assert(valid.den == 90000);

    openmedia::RationalTime rawZero{5, 0};
    assert(std::abs(rawZero.toSeconds() - 5.0) < 1e-9);
}

void testInvalidPathsAndUninitializedCalls() {
    openmedia::CoreDemuxer demuxer;
    assert(!demuxer.isOpen());
    assert(!demuxer.isQuieted());
    assert(!demuxer.resetFirstTimestamp());
    assert(demuxer.lastError().empty());
    assert(demuxer.diagnosticMessage().empty());
    assert(demuxer.getStreams().empty());

    assert(demuxer.openFile("") == openmedia::CoreStatus::InvalidArgument);
    assert(!demuxer.lastError().empty());
    assert(demuxer.openFile("/nonexistent/openmedia_missing_fixture_12345.mkv") ==
           openmedia::CoreStatus::OpenFailed);
    assert(!demuxer.lastError().empty());
    assert(!demuxer.isOpen());

    openmedia::EncodedPacket pkt;
    assert(demuxer.readNextPacket(pkt) == openmedia::CoreStatus::NotInitialized);
    assert(demuxer.seekToTimestamp(0, 0) == openmedia::CoreStatus::NotInitialized);
    assert(demuxer.quietFile() == openmedia::CoreStatus::NotInitialized);
    assert(demuxer.reopenIfQuieted() == openmedia::CoreStatus::NotInitialized);

    openmedia::CoreDemuxer movedDemuxer(std::move(demuxer));
    assert(demuxer.getStreams().empty());
    assert(!demuxer.isOpen());
}

void verifyDiscoveredStreams(const std::vector<openmedia::StreamInfo>& streams) {
    assert(!streams.empty());
    for (const auto& s : streams) {
        assert(s.timeBase.den >= 1);
        assert(s.frameRate.den >= 1);
    }
}

void createValidTestMkv(const std::string& path) {
    openmedia::CoreMuxer muxer;
    assert(muxer.openContainer(path, "matroska") == openmedia::CoreStatus::Ok);

    openmedia::MuxerStreamConfig vCfg;
    vCfg.info.type = openmedia::MediaType::Video;
    vCfg.info.codec = openmedia::CodecId::AV1;
    vCfg.info.width = 64;
    vCfg.info.height = 64;
    vCfg.info.timeBase = openmedia::RationalTime{1001, 24000};
    vCfg.info.frameRate = openmedia::RationalTime{24000, 1001};
    vCfg.info.extradata = kValidAv1Extradata;
    assert(muxer.addStream(vCfg) == 0);

    for (int i = 0; i < 4; ++i) {
        openmedia::MuxerStreamConfig aCfg;
        aCfg.info.type = openmedia::MediaType::Audio;
        aCfg.info.codec = openmedia::CodecId::Opus;
        aCfg.info.sampleRate = 48000;
        aCfg.info.channels = 2;
        aCfg.info.timeBase = openmedia::RationalTime{1, 48000};
        assert(muxer.addStream(aCfg) == i + 1);
    }

    assert(muxer.writeHeader() == openmedia::CoreStatus::Ok);
    openmedia::EncodedPacket pkt;
    pkt.streamIndex = 0;
    pkt.pts = 0;
    pkt.dts = 0;
    pkt.duration = 1001;
    pkt.isKeyFrame = true;
    pkt.data = kValidAv1KeyPacket;
    assert(muxer.writeInterleavedPacket(pkt) == openmedia::CoreStatus::Ok);

    for (int i = 1; i <= 4; ++i) {
        openmedia::EncodedPacket aPkt;
        aPkt.streamIndex = i;
        aPkt.pts = 0;
        aPkt.dts = 0;
        aPkt.duration = 960;
        aPkt.isKeyFrame = true;
        aPkt.data = {0xF8U, 0xFFU, 0xFEU};
        assert(muxer.writeInterleavedPacket(aPkt) == openmedia::CoreStatus::Ok);
    }
    assert(muxer.writeTrailerAndClose() == openmedia::CoreStatus::Ok);
}

void testQuietAndReopenLifecycle() {
    openmedia::CoreDemuxer demuxer;
    const auto tempPath =
        std::filesystem::temp_directory_path() / "openmedia_test_5streams_valid.mkv";
    createValidTestMkv(tempPath.string());

    assert(demuxer.openFile(tempPath.string()) == openmedia::CoreStatus::Ok);
    assert(demuxer.isOpen());
    assert(!demuxer.isQuieted());
    assert(demuxer.lastError().empty());

    const auto& streams = demuxer.getStreams();
    assert(streams.size() == 5);
    verifyDiscoveredStreams(streams);
    assert(streams[0].type == openmedia::MediaType::Video);
    assert(streams[0].codec == openmedia::CodecId::AV1);
    assert(streams[1].type == openmedia::MediaType::Audio);
    assert(streams[1].codec == openmedia::CodecId::Opus);

    assert(demuxer.quietFile() == openmedia::CoreStatus::Ok);
    assert(demuxer.isQuieted());
    assert(demuxer.getStreams().size() == 5);

    assert(demuxer.reopenIfQuieted() == openmedia::CoreStatus::Ok);
    assert(!demuxer.isQuieted());
    assert(demuxer.getStreams().size() == 5);

    assert(demuxer.seekToTimestamp(0, 0) == openmedia::CoreStatus::Ok);
    assert(demuxer.seekToTimestamp(4, 0) == openmedia::CoreStatus::Ok);
    assert(demuxer.seekToTimestamp(5, 0) == openmedia::CoreStatus::InvalidArgument);
    assert(demuxer.seekToTimestamp(-1, 0) == openmedia::CoreStatus::InvalidArgument);

    openmedia::EncodedPacket pkt;
    const auto readStatus = demuxer.readNextPacket(pkt);
    assert(readStatus == openmedia::CoreStatus::Ok ||
           readStatus == openmedia::CoreStatus::EndOfFile);

    demuxer.close();
    assert(!demuxer.isOpen());
    std::filesystem::remove(tempPath);
}

void testStartTimeNormalizationNonZeroStart() {
    const auto tempPath =
        std::filesystem::temp_directory_path() / "openmedia_test_nonzero_start.mkv";
    writeDummyFixtureFile(tempPath);

    openmedia::CoreDemuxer demuxer;
    assert(demuxer.openFile(tempPath.string()) == openmedia::CoreStatus::Ok);

    openmedia::EncodedPacket pkt0;
    assert(demuxer.readNextPacket(pkt0) == openmedia::CoreStatus::Ok);
    assert(pkt0.streamIndex == 0);
    assert(pkt0.isKeyFrame);
    assert(pkt0.pts == 0);
    assert(pkt0.dts == 0);

    openmedia::EncodedPacket pkt1;
    assert(demuxer.readNextPacket(pkt1) == openmedia::CoreStatus::Ok);
    assert(pkt1.pts == 1001);
    assert(pkt1.dts == 1001);

    openmedia::EncodedPacket eofPkt;
    assert(demuxer.readNextPacket(eofPkt) == openmedia::CoreStatus::EndOfFile);

    assert(demuxer.seekToTimestamp(0, 0) == openmedia::CoreStatus::Ok);
    openmedia::EncodedPacket rewoundPkt;
    assert(demuxer.readNextPacket(rewoundPkt) == openmedia::CoreStatus::Ok);
    assert(rewoundPkt.pts == 0);

    demuxer.close();
    std::filesystem::remove(tempPath);
}

void testResetFirstTimestampOption() {
    const auto tempPath =
        std::filesystem::temp_directory_path() / "openmedia_test_raw_offset.mkv";
    writeDummyFixtureFile(tempPath);

    openmedia::CoreDemuxer demuxer;
    demuxer.setResetFirstTimestamp(true);
    assert(demuxer.resetFirstTimestamp());
    assert(demuxer.openFile(tempPath.string()) == openmedia::CoreStatus::Ok);

    openmedia::EncodedPacket pkt0;
    assert(demuxer.readNextPacket(pkt0) == openmedia::CoreStatus::Ok);
    assert(pkt0.pts == 0);
    assert(pkt0.dts == 0);

    openmedia::EncodedPacket pkt1;
    assert(demuxer.readNextPacket(pkt1) == openmedia::CoreStatus::Ok);
    assert(pkt1.pts == 1001);
    assert(pkt1.dts == 1001);

    demuxer.setResetFirstTimestamp(false);
    assert(!demuxer.resetFirstTimestamp());
    assert(demuxer.openFile(tempPath.string()) == openmedia::CoreStatus::Ok);
    openmedia::EncodedPacket rawPkt;
    assert(demuxer.readNextPacket(rawPkt) == openmedia::CoreStatus::Ok);
    assert(rawPkt.pts == 4500);

    demuxer.close();
    std::filesystem::remove(tempPath);
}

void testEmptyVideoNoDecodableFrames() {
    const auto tempPath =
        std::filesystem::temp_directory_path() / "openmedia_test_empty_video.mkv";
    writeDummyFixtureFile(tempPath);

    openmedia::CoreDemuxer demuxer;
    const auto status = demuxer.openFile(tempPath.string());
    assert(status == openmedia::CoreStatus::NoDecodableFrames);
    assert(demuxer.lastError() == "No decodable video frames in file");
    assert(demuxer.diagnosticMessage() == "No decodable video frames in file");
    assert(!demuxer.isOpen());
    assert(demuxer.getStreams().empty());

    assert(std::filesystem::remove(tempPath));
    assert(!std::filesystem::exists(tempPath));
}

void testImmediateFileUnlockOnFatalAudioProbeFailure() {
    const auto tempPath =
        std::filesystem::temp_directory_path() / "openmedia_test_fatal_audio.mkv";
    writeDummyFixtureFile(tempPath);

    openmedia::CoreDemuxer demuxer;
    const auto status = demuxer.openFile(tempPath.string());
    assert(status == openmedia::CoreStatus::OpenFailed);
    assert(!demuxer.lastError().empty());
    assert(!demuxer.isOpen());
    assert(demuxer.getStreams().empty());

    assert(std::filesystem::remove(tempPath));
    assert(!std::filesystem::exists(tempPath));
}

} // namespace

int main() {
    testRationalTimebaseSanitization();
    testInvalidPathsAndUninitializedCalls();
    testQuietAndReopenLifecycle();
    testStartTimeNormalizationNonZeroStart();
    testResetFirstTimestampOption();
    testEmptyVideoNoDecodableFrames();
    testImmediateFileUnlockOnFatalAudioProbeFailure();
    std::cout << "[PASS] TC-1.2.1 CoreDemuxer unit tests passed.\n";
    return 0;
}
