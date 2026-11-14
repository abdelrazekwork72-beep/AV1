/// @file test_demuxer.cpp
/// @brief Standalone unit tests (TC-1.2.1) for CoreDemuxer.

#include "openmedia/demuxer.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

namespace {

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
    assert(demuxer.getStreams().empty());

    assert(demuxer.openFile("") == openmedia::CoreStatus::InvalidArgument);
    assert(demuxer.openFile("/nonexistent/openmedia_missing_fixture_12345.mkv") ==
           openmedia::CoreStatus::OpenFailed);
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

void testQuietAndReopenLifecycle() {
    openmedia::CoreDemuxer demuxer;
    const char* fixtureEnv = std::getenv("OPENMEDIA_TEST_MKV_PATH");

#ifdef OPENMEDIA_HAS_FFMPEG
    if (fixtureEnv != nullptr && std::filesystem::exists(fixtureEnv)) {
        assert(demuxer.openFile(fixtureEnv) == openmedia::CoreStatus::Ok);
        assert(demuxer.isOpen());
        assert(!demuxer.isQuieted());
        const auto& streams = demuxer.getStreams();
        verifyDiscoveredStreams(streams);
        const size_t streamCount = streams.size();

        assert(demuxer.quietFile() == openmedia::CoreStatus::Ok);
        assert(demuxer.isQuieted());
        assert(demuxer.getStreams().size() == streamCount);

        assert(demuxer.reopenIfQuieted() == openmedia::CoreStatus::Ok);
        assert(!demuxer.isQuieted());
        assert(demuxer.getStreams().size() == streamCount);

        assert(demuxer.seekToTimestamp(0, 0) == openmedia::CoreStatus::Ok);
        openmedia::EncodedPacket pkt;
        const auto readStatus = demuxer.readNextPacket(pkt);
        assert(readStatus == openmedia::CoreStatus::Ok ||
               readStatus == openmedia::CoreStatus::EndOfFile);
    }
#else
    (void)fixtureEnv;
    const auto tempPath =
        std::filesystem::temp_directory_path() / "openmedia_test_5streams.mkv";
    {
        std::ofstream ofs(tempPath, std::ios::binary);
        ofs << "\x1A\x45\xDF\xA3matroska";
    }
    assert(demuxer.openFile(tempPath.string()) == openmedia::CoreStatus::Ok);
    assert(demuxer.isOpen());
    assert(!demuxer.isQuieted());

    const auto& streams = demuxer.getStreams();
    assert(streams.size() == 5);
    verifyDiscoveredStreams(streams);
    assert(streams[0].type == openmedia::MediaType::Video);
    assert(streams[0].codec == openmedia::CodecId::AV1);
    assert(streams[0].frameRate.num == 24000 && streams[0].frameRate.den == 1001);
    assert(streams[1].type == openmedia::MediaType::Audio);
    assert(streams[1].timeBase.num == 1 && streams[1].timeBase.den == 48000);

    assert(demuxer.quietFile() == openmedia::CoreStatus::Ok);
    assert(demuxer.isQuieted());
    assert(demuxer.getStreams().size() == 5);

    assert(demuxer.reopenIfQuieted() == openmedia::CoreStatus::Ok);
    assert(!demuxer.isQuieted());
    assert(demuxer.getStreams().size() == 5);

    openmedia::EncodedPacket pkt;
    assert(demuxer.readNextPacket(pkt) == openmedia::CoreStatus::EndOfFile);
    assert(demuxer.seekToTimestamp(0, 0) == openmedia::CoreStatus::Ok);
    assert(demuxer.seekToTimestamp(4, 48000) == openmedia::CoreStatus::Ok);
    assert(demuxer.seekToTimestamp(5, 0) == openmedia::CoreStatus::InvalidArgument);
    assert(demuxer.seekToTimestamp(-1, 0) == openmedia::CoreStatus::InvalidArgument);

    demuxer.close();
    assert(!demuxer.isOpen());
    std::filesystem::remove(tempPath);
#endif
}

} // namespace

int main() {
    testRationalTimebaseSanitization();
    testInvalidPathsAndUninitializedCalls();
    testQuietAndReopenLifecycle();
    std::cout << "[PASS] TC-1.2.1 CoreDemuxer unit tests passed.\n";
    return 0;
}
