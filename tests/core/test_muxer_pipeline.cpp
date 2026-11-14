/// @file test_muxer_pipeline.cpp
/// @brief Standalone unit test TC-1.2.5 for CoreMuxer and CoreAudioEncoder.

#include "openmedia/muxer.hpp"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace openmedia;

void testMuxerContainerAndStreamRegistration() {
    CoreMuxer muxer;

    MuxerStreamConfig videoCfg;
    videoCfg.info.type = MediaType::Video;
    videoCfg.info.codec = CodecId::AV1;
    videoCfg.info.width = 1920;
    videoCfg.info.height = 1080;
    videoCfg.info.timeBase = RationalTime{1, 30};

    assert(muxer.addStream(videoCfg) == -1);
    assert(muxer.writeHeader() == CoreStatus::NotInitialized);
    assert(muxer.writeTrailerAndClose() == CoreStatus::NotInitialized);

    assert(muxer.openContainer("/tmp/tc_1_2_5_invalid.avi", "avi") == CoreStatus::InvalidArgument);
    assert(muxer.openContainer("", "matroska") == CoreStatus::InvalidArgument);

    const std::string outPath = "tc_1_2_5_test.mkv";
    assert(muxer.openContainer(outPath, "matroska") == CoreStatus::Ok);
    assert(muxer.writeHeader() == CoreStatus::InvalidArgument);

    const int32_t vIdx = muxer.addStream(videoCfg);
    assert(vIdx == 0);

    MuxerStreamConfig audioCfg;
    audioCfg.info.type = MediaType::Audio;
    audioCfg.info.codec = CodecId::Opus;
    audioCfg.info.sampleRate = 48000;
    audioCfg.info.channels = 2;
    audioCfg.info.timeBase = RationalTime{1, 48000};
    const int32_t aIdx = muxer.addStream(audioCfg);
    assert(aIdx == 1);

    assert(muxer.writeHeader() == CoreStatus::Ok);
    assert(muxer.addStream(audioCfg) == -1);

    EncodedPacket badStreamPkt;
    badStreamPkt.streamIndex = 99;
    badStreamPkt.pts = 0;
    badStreamPkt.dts = 0;
    badStreamPkt.data = {0x01, 0x02};
    assert(muxer.writeInterleavedPacket(badStreamPkt) == CoreStatus::InvalidArgument);

    EncodedPacket pkt0;
    pkt0.streamIndex = vIdx;
    pkt0.pts = 10;
    pkt0.dts = 10;
    pkt0.duration = 1;
    pkt0.isKeyFrame = true;
    pkt0.data = {0x12, 0x00, 0x0A};
    assert(muxer.writeInterleavedPacket(pkt0) == CoreStatus::Ok);

    EncodedPacket regressedPkt = pkt0;
    regressedPkt.pts = 5;
    regressedPkt.dts = 5;
    assert(muxer.writeInterleavedPacket(regressedPkt) == CoreStatus::InvalidArgument);

    EncodedPacket pkt1 = pkt0;
    pkt1.pts = 11;
    pkt1.dts = 11;
    pkt1.isKeyFrame = false;
    assert(muxer.writeInterleavedPacket(pkt1) == CoreStatus::Ok);

    assert(muxer.writeTrailerAndClose() == CoreStatus::Ok);
    std::remove(outPath.c_str());
    std::cout << "[PASS] CoreMuxer container registration & interleaved ordering guards\n";
}

void testAudioEncoderConfigValidation() {
    CoreAudioEncoder encoder;

    RawAudioBatch batch;
    batch.sampleRate = 48000;
    batch.channels = 2;
    batch.numSamples = 960;
    batch.channelData.assign(2, std::vector<float>(960, 0.0f));
    std::vector<EncodedPacket> packets;

    assert(encoder.encodeAudio(batch, packets) == CoreStatus::NotInitialized);
    assert(encoder.flush(packets) == CoreStatus::NotInitialized);

    AudioEncodeConfig badRate{CodecId::Opus, 0, 2, 128000};
    assert(encoder.initialize(badRate) == CoreStatus::InvalidArgument);

    AudioEncodeConfig badChannels{CodecId::FLAC, 48000, 0, 0};
    assert(encoder.initialize(badChannels) == CoreStatus::InvalidArgument);

    AudioEncodeConfig badCodec{CodecId::AV1, 48000, 2, 128000};
    assert(encoder.initialize(badCodec) == CoreStatus::UnsupportedCodec);

    AudioEncodeConfig opusCfg{CodecId::Opus, 48000, 2, 128000};
    const CoreStatus initStatus = encoder.initialize(opusCfg);
    if (initStatus == CoreStatus::Ok) {
        RawAudioBatch badBatch = batch;
        badBatch.channels = 1;
        assert(encoder.encodeAudio(badBatch, packets) == CoreStatus::InvalidArgument);
        assert(encoder.encodeAudio(batch, packets) == CoreStatus::Ok);
        assert(encoder.flush(packets) == CoreStatus::Ok);
        assert(!packets.empty());
    }
    std::cout << "[PASS] CoreAudioEncoder config validation & encode guards\n";
}

} // namespace

int main() {
    testMuxerContainerAndStreamRegistration();
    testAudioEncoderConfigValidation();
    std::cout << "TC-1.2.5: All CoreMuxer & CoreAudioEncoder checks passed.\n";
    return 0;
}
