/// @file test_video_encoder.cpp
/// @brief Standalone unit tests (TC-1.2.4) for CoreVideoEncoder.

#include "openmedia/video_encoder.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

using namespace openmedia;

namespace {

RawVideoFrame makeSynthetic10BitFrame(int32_t width, int32_t height, int64_t pts) {
    RawVideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.bitDepth = 10;
    frame.format = PixelFormat::YUV420P10LE;
    frame.colorRange = ColorRange::Limited;
    frame.colorSpace = ColorSpace::BT709;
    frame.colorTransfer = ColorTransfer::BT709;
    frame.pts = pts;
    frame.duration = 1;

    const uint32_t yStride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width) * 2U);
    const uint32_t uvStride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width / 2) * 2U);
    frame.rowBytes[0] = yStride;
    frame.rowBytes[1] = uvStride;
    frame.rowBytes[2] = uvStride;

    frame.planes[0].assign(static_cast<size_t>(yStride) * height, static_cast<uint8_t>(pts & 0xFF));
    frame.planes[1].assign(static_cast<size_t>(uvStride) * (height / 2), 0x02);
    frame.planes[2].assign(static_cast<size_t>(uvStride) * (height / 2), 0x02);
    return frame;
}

void testNormalizedCrfMapping() {
    assert(CoreVideoEncoder::normalizeCrfToBaseQIndex(0) == 0);
    assert(CoreVideoEncoder::normalizeCrfToBaseQIndex(63) == 255);
    assert(CoreVideoEncoder::normalizeCrfToBaseQIndex(-10) == 0);
    assert(CoreVideoEncoder::normalizeCrfToBaseQIndex(99) == 255);

    assert(CoreVideoEncoder::normalizeCrfToVp9Cq(0) == 0);
    assert(CoreVideoEncoder::normalizeCrfToVp9Cq(63) == 51);
    assert(CoreVideoEncoder::normalizeCrfToVp9Cq(-5) == 0);
    assert(CoreVideoEncoder::normalizeCrfToVp9Cq(80) == 51);

    int32_t prevAv1 = -1;
    int32_t prevVp9 = -1;
    for (int32_t crf = 0; crf <= 63; ++crf) {
        const int32_t qIdx = CoreVideoEncoder::normalizeCrfToBaseQIndex(crf);
        const int32_t cq = CoreVideoEncoder::normalizeCrfToVp9Cq(crf);
        assert(qIdx >= prevAv1 && qIdx <= 255);
        assert(cq >= prevVp9 && cq <= 51);
        prevAv1 = qIdx;
        prevVp9 = cq;
    }
}

void testConfigValidationAndGuards() {
    CoreVideoEncoder encoder;
    std::vector<EncodedPacket> packets;

    RawVideoFrame dummy = makeSynthetic10BitFrame(128, 128, 0);
    assert(encoder.encodeFrame(dummy, packets) == CoreStatus::NotInitialized);
    assert(encoder.flush(packets) == CoreStatus::NotInitialized);
    assert(encoder.getExtradata().empty());

    VideoEncodeConfig badCodec;
    badCodec.codec = CodecId::H264;
    assert(encoder.initialize(badCodec) == CoreStatus::UnsupportedCodec);

    VideoEncodeConfig badDims;
    badDims.width = 0;
    badDims.height = 128;
    assert(encoder.initialize(badDims) == CoreStatus::InvalidArgument);

    VideoEncodeConfig badBitDepth;
    badBitDepth.bitDepth = 9;
    assert(encoder.initialize(badBitDepth) == CoreStatus::InvalidArgument);

    VideoEncodeConfig badPixFmt;
    badPixFmt.pixelFormat = PixelFormat::Unknown;
    assert(encoder.initialize(badPixFmt) == CoreStatus::InvalidArgument);

    VideoEncodeConfig badFps;
    badFps.frameRate = RationalTime{0, 0};
    assert(encoder.initialize(badFps) == CoreStatus::InvalidArgument);

    CoreVideoEncoder movedEncoder(std::move(encoder));
    assert(encoder.initialize(VideoEncodeConfig{}) == CoreStatus::NotInitialized);
    assert(encoder.encodeFrame(dummy, packets) == CoreStatus::NotInitialized);
    assert(encoder.flush(packets) == CoreStatus::NotInitialized);
    assert(encoder.getExtradata().empty());
}

void testEncodeAndFlushGop() {
    CoreVideoEncoder encoder;
    VideoEncodeConfig cfg;
    cfg.codec = CodecId::AV1;
    cfg.width = 128;
    cfg.height = 128;
    cfg.frameRate = RationalTime{30, 1};
    cfg.timeBase = RationalTime{1, 30};
    cfg.pixelFormat = PixelFormat::YUV420P10LE;
    cfg.bitDepth = 10;
    cfg.crf = 32;
    cfg.preset = 10;
    cfg.gopSize = 4;

    const CoreStatus initStatus = encoder.initialize(cfg);
    assert(initStatus == CoreStatus::Ok);

    std::vector<EncodedPacket> packets;
    RawVideoFrame mismatched = makeSynthetic10BitFrame(64, 64, 0);
    assert(encoder.encodeFrame(mismatched, packets) == CoreStatus::InvalidArgument);

    RawVideoFrame narrowStride = makeSynthetic10BitFrame(128, 128, 0);
    narrowStride.rowBytes[0] = 64U;
    assert(encoder.encodeFrame(narrowStride, packets) == CoreStatus::InvalidArgument);

    constexpr int32_t kFrameCount = 6;
    for (int32_t i = 0; i < kFrameCount; ++i) {
        RawVideoFrame frame = makeSynthetic10BitFrame(128, 128, i);
        assert(encoder.encodeFrame(frame, packets) == CoreStatus::Ok);
    }

    assert(encoder.flush(packets) == CoreStatus::Ok);
    assert(packets.size() >= static_cast<size_t>(kFrameCount));
    assert(packets.front().isKeyFrame);
    for (const auto& pkt : packets) {
        assert(!pkt.data.empty());
    }

    const size_t countAfterFirstFlush = packets.size();
    assert(encoder.flush(packets) == CoreStatus::Ok);
    assert(packets.size() == countAfterFirstFlush);
}

} // namespace

int main() {
    testNormalizedCrfMapping();
    testConfigValidationAndGuards();
    testEncodeAndFlushGop();
    std::cout << "TC-1.2.4 CoreVideoEncoder tests passed.\n";
    return 0;
}
