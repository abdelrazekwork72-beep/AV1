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

RawVideoFrame makeSyntheticFrameForFormat(PixelFormat fmt, int32_t width, int32_t height, int64_t pts) {
    RawVideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.format = fmt;
    frame.pts = pts;
    frame.duration = 1;

    if (fmt == PixelFormat::BGRA8 || fmt == PixelFormat::RGBA8) {
        frame.bitDepth = 8;
        const uint32_t stride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width) * 4U);
        frame.rowBytes[0] = stride;
        frame.planes[0].assign(static_cast<size_t>(stride) * height, 0x40);
        return frame;
    }
    if (fmt == PixelFormat::NV12) {
        frame.bitDepth = 8;
        const uint32_t stride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width));
        frame.rowBytes[0] = stride;
        frame.rowBytes[1] = stride;
        frame.planes[0].assign(static_cast<size_t>(stride) * height, 0x10);
        frame.planes[1].assign(static_cast<size_t>(stride) * (height / 2), 0x80);
        return frame;
    }
    if (fmt == PixelFormat::YUV444P10LE) {
        frame.bitDepth = 10;
        const uint32_t stride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width) * 2U);
        frame.rowBytes[0] = stride;
        frame.rowBytes[1] = stride;
        frame.rowBytes[2] = stride;
        frame.planes[0].assign(static_cast<size_t>(stride) * height, 0x10);
        frame.planes[1].assign(static_cast<size_t>(stride) * height, 0x02);
        frame.planes[2].assign(static_cast<size_t>(stride) * height, 0x02);
        return frame;
    }
    if (fmt == PixelFormat::YUV444P) {
        frame.bitDepth = 8;
        const uint32_t stride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width));
        frame.rowBytes[0] = stride;
        frame.rowBytes[1] = stride;
        frame.rowBytes[2] = stride;
        frame.planes[0].assign(static_cast<size_t>(stride) * height, 0x10);
        frame.planes[1].assign(static_cast<size_t>(stride) * height, 0x80);
        frame.planes[2].assign(static_cast<size_t>(stride) * height, 0x80);
        return frame;
    }
    frame.bitDepth = 8;
    const uint32_t yStride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width));
    const uint32_t uvStride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width / 2));
    frame.rowBytes[0] = yStride;
    frame.rowBytes[1] = uvStride;
    frame.rowBytes[2] = uvStride;
    frame.planes[0].assign(static_cast<size_t>(yStride) * height, 0x10);
    frame.planes[1].assign(static_cast<size_t>(uvStride) * (height / 2), 0x80);
    frame.planes[2].assign(static_cast<size_t>(uvStride) * (height / 2), 0x80);
    return frame;
}

void testNormalizedCrfAndPtsMapping() {
    assert(CoreVideoEncoder::normalizeCrfToBaseQIndex(0) == 0);
    assert(CoreVideoEncoder::normalizeCrfToBaseQIndex(63) == 255);
    assert(CoreVideoEncoder::normalizeCrfToBaseQIndex(-10) == 0);
    assert(CoreVideoEncoder::normalizeCrfToBaseQIndex(99) == 255);

    assert(CoreVideoEncoder::normalizeCrfToVp9Cq(0) == 0);
    assert(CoreVideoEncoder::normalizeCrfToVp9Cq(63) == 51);
    assert(CoreVideoEncoder::normalizeCrfToVp9Cq(-5) == 0);
    assert(CoreVideoEncoder::normalizeCrfToVp9Cq(80) == 51);

    assert(CoreVideoEncoder::normalizePts(-42) == 0);
    assert(CoreVideoEncoder::normalizePts(0) == 0);
    assert(CoreVideoEncoder::normalizePts(12345) == 12345);

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

    VideoEncodeConfig badGop;
    badGop.gopSize = -1;
    assert(encoder.initialize(badGop) == CoreStatus::InvalidArgument);

    VideoEncodeConfig badBitrate;
    badBitrate.bitrateBps = -500;
    assert(encoder.initialize(badBitrate) == CoreStatus::InvalidArgument);

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

    CoreVideoEncoder moveAssigned;
    moveAssigned = std::move(movedEncoder);
    assert(movedEncoder.initialize(VideoEncodeConfig{}) == CoreStatus::NotInitialized);
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
    assert(!encoder.getExtradata().empty());

    std::vector<EncodedPacket> packets;
    RawVideoFrame mismatched = makeSynthetic10BitFrame(64, 64, 0);
    assert(encoder.encodeFrame(mismatched, packets) == CoreStatus::InvalidArgument);

    RawVideoFrame narrowStride = makeSynthetic10BitFrame(128, 128, 0);
    narrowStride.rowBytes[0] = 64U;
    assert(encoder.encodeFrame(narrowStride, packets) == CoreStatus::InvalidArgument);

    RawVideoFrame emptyPlane = makeSynthetic10BitFrame(128, 128, 0);
    emptyPlane.planes[1].clear();
    assert(encoder.encodeFrame(emptyPlane, packets) == CoreStatus::InvalidArgument);

    // Verify negative PTS normalizes to 0 and zero-PTS is preserved.
    RawVideoFrame negativePtsFrame = makeSynthetic10BitFrame(128, 128, -10);
    assert(encoder.encodeFrame(negativePtsFrame, packets) == CoreStatus::Ok);

    constexpr int32_t kFrameCount = 5;
    for (int32_t i = 1; i <= kFrameCount; ++i) {
        RawVideoFrame frame = makeSynthetic10BitFrame(128, 128, i);
        assert(encoder.encodeFrame(frame, packets) == CoreStatus::Ok);
    }

    assert(encoder.flush(packets) == CoreStatus::Ok);
    assert(packets.size() >= static_cast<size_t>(kFrameCount + 1));
    assert(packets.front().pts == 0);
    assert(packets.front().isKeyFrame);
    for (const auto& pkt : packets) {
        assert(!pkt.data.empty());
        assert(pkt.pts >= 0);
    }

    // Post-flush encodeFrame must fail cleanly; second flush must be idempotent.
    RawVideoFrame afterEof = makeSynthetic10BitFrame(128, 128, 99);
    assert(encoder.encodeFrame(afterEof, packets) == CoreStatus::EncodeFailed);

    const size_t countAfterFirstFlush = packets.size();
    assert(encoder.flush(packets) == CoreStatus::Ok);
    assert(packets.size() == countAfterFirstFlush);

    // Re-initialization resets flushed state.
    assert(encoder.initialize(cfg) == CoreStatus::Ok);
    std::vector<EncodedPacket> reinitPackets;
    assert(encoder.encodeFrame(makeSynthetic10BitFrame(128, 128, 0), reinitPackets) == CoreStatus::Ok);
    assert(encoder.flush(reinitPackets) == CoreStatus::Ok);
    assert(!reinitPackets.empty());
}

void testVp9HardwareFallbackAndPixelFormats() {
    // Hardware preference must safely fall back to CPU (libsvtav1/libaom-av1) when no NVENC hardware is available.
    CoreVideoEncoder av1HwFallback;
    VideoEncodeConfig hwCfg;
    hwCfg.codec = CodecId::AV1;
    hwCfg.width = 64;
    hwCfg.height = 64;
    hwCfg.pixelFormat = PixelFormat::YUV420P;
    hwCfg.bitDepth = 8;
    hwCfg.preferHardware = true;
    hwCfg.bitrateBps = 500000;
    assert(av1HwFallback.initialize(hwCfg) == CoreStatus::Ok);
    std::vector<EncodedPacket> hwPackets;
    RawVideoFrame keyFrame = makeSyntheticFrameForFormat(PixelFormat::YUV420P, 64, 64, 0);
    keyFrame.isKeyFrame = true;
    assert(av1HwFallback.encodeFrame(keyFrame, hwPackets) == CoreStatus::Ok);
    assert(av1HwFallback.flush(hwPackets) == CoreStatus::Ok);
    assert(hwPackets.size() == 1U);
    assert(hwPackets.front().isKeyFrame);

    // VP9 with explicit bitrate and 8-bit YUV420P / YUV444P / 10-bit YUV444P10LE paths.
    CoreVideoEncoder vp9Encoder;
    VideoEncodeConfig vp9Cfg;
    vp9Cfg.codec = CodecId::VP9;
    vp9Cfg.width = 64;
    vp9Cfg.height = 64;
    vp9Cfg.pixelFormat = PixelFormat::YUV420P;
    vp9Cfg.bitDepth = 8;
    vp9Cfg.crf = 30;
    vp9Cfg.gopSize = 0; // Edge case: zero GOP size.
    assert(vp9Encoder.initialize(vp9Cfg) == CoreStatus::Ok);
    std::vector<EncodedPacket> vp9Packets;
    assert(vp9Encoder.encodeFrame(makeSyntheticFrameForFormat(PixelFormat::YUV420P, 64, 64, 0), vp9Packets) == CoreStatus::Ok);
    assert(vp9Encoder.flush(vp9Packets) == CoreStatus::Ok);
    assert(vp9Packets.size() == 1U);

    // Planar 4:4:4 and 10-bit 4:4:4 validation.
    vp9Cfg.pixelFormat = PixelFormat::YUV444P;
    assert(vp9Encoder.initialize(vp9Cfg) == CoreStatus::Ok);
    vp9Packets.clear();
    assert(vp9Encoder.encodeFrame(makeSyntheticFrameForFormat(PixelFormat::YUV444P, 64, 64, 0), vp9Packets) == CoreStatus::Ok);
    assert(vp9Encoder.flush(vp9Packets) == CoreStatus::Ok);
    assert(vp9Packets.size() == 1U);

    vp9Cfg.pixelFormat = PixelFormat::YUV444P10LE;
    vp9Cfg.bitDepth = 10;
    assert(vp9Encoder.initialize(vp9Cfg) == CoreStatus::Ok);
    vp9Packets.clear();
    RawVideoFrame yuv444p10Frame = makeSyntheticFrameForFormat(PixelFormat::YUV444P10LE, 64, 64, 0);
    yuv444p10Frame.isKeyFrame = true;
    assert(vp9Encoder.encodeFrame(yuv444p10Frame, vp9Packets) == CoreStatus::Ok);
    assert(vp9Encoder.flush(vp9Packets) == CoreStatus::Ok);
    assert(vp9Packets.size() == 1U);
    assert(vp9Packets.front().isKeyFrame);
}

} // namespace

int main() {
    testNormalizedCrfAndPtsMapping();
    testConfigValidationAndGuards();
    testEncodeAndFlushGop();
    testVp9HardwareFallbackAndPixelFormats();
    std::cout << "TC-1.2.4 CoreVideoEncoder tests passed.\n";
    return 0;
}
