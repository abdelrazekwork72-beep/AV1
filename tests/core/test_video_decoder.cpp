/// @file test_video_decoder.cpp
/// @brief Standalone unit tests (TC-1.2.2) for CoreVideoDecoder.

#include "openmedia/video_decoder.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int gFailureCount = 0;

void check(bool condition, const char* testName, const char* detail) {
    if (condition) {
        std::cout << "[PASS] " << testName << ": " << detail << "\n";
        return;
    }
    std::cerr << "[FAIL] " << testName << ": " << detail << "\n";
    ++gFailureCount;
}

struct AlignmentCase {
    const char* label;
    int32_t width;
    int32_t height;
    int32_t bitDepth;
    openmedia::PixelFormat format;
    uint32_t expectedYStride;
    uint32_t expectedUVStride;
};

bool hasValidPlaneAlignment(const openmedia::RawVideoFrame& frame) noexcept {
    if (frame.rowBytes[0] % 16U != 0U) {
        return false;
    }
    return (frame.rowBytes[1] % 16U == 0U) && (frame.rowBytes[2] % 16U == 0U);
}

bool matchesExpectedStrides(const openmedia::RawVideoFrame& frame,
                            const AlignmentCase& c) noexcept {
    if (frame.rowBytes[0] != c.expectedYStride) {
        return false;
    }
    return (frame.rowBytes[1] == c.expectedUVStride) &&
           (frame.rowBytes[2] == c.expectedUVStride);
}

void verifySingleAlignmentCase(const AlignmentCase& c) {
    const uint32_t bytesPerSample = (c.bitDepth > 8) ? 2U : 1U;
    const uint32_t yWidthBytes = static_cast<uint32_t>(c.width) * bytesPerSample;
    const bool is444 = (c.format == openmedia::PixelFormat::YUV444P12LE);
    const uint32_t uvWidthPixels = is444
        ? static_cast<uint32_t>(c.width)
        : static_cast<uint32_t>((c.width + 1) >> 1);
    const uint32_t uvWidthBytes = uvWidthPixels * bytesPerSample;

    openmedia::RawVideoFrame frame;
    frame.width = c.width;
    frame.height = c.height;
    frame.bitDepth = c.bitDepth;
    frame.format = c.format;
    frame.rowBytes[0] = openmedia::RawVideoFrame::alignRowStride16(yWidthBytes);
    frame.rowBytes[1] = openmedia::RawVideoFrame::alignRowStride16(uvWidthBytes);
    frame.rowBytes[2] = openmedia::RawVideoFrame::alignRowStride16(uvWidthBytes);

    const bool ok = hasValidPlaneAlignment(frame) && matchesExpectedStrides(frame, c);
    check(ok, "TC-1.2.2 Alignment", c.label);
}

/// @brief Verifies RawVideoFrame::alignRowStride16 and 16-byte row alignment across 8/10/12-bit formats.
void test16ByteRowAlignment() {
    const std::vector<AlignmentCase> cases = {
        {"8-bit YUV420P 1920x1080", 1920, 1080, 8, openmedia::PixelFormat::YUV420P, 1920U, 960U},
        {"8-bit YUV420P 1919x1080 (odd)", 1919, 1080, 8, openmedia::PixelFormat::YUV420P, 1920U, 960U},
        {"10-bit YUV420P10LE 1920x1080", 1920, 1080, 10, openmedia::PixelFormat::YUV420P10LE, 3840U, 1920U},
        {"10-bit YUV420P10LE 1281x720 (unaligned)", 1281, 720, 10, openmedia::PixelFormat::YUV420P10LE, 2576U, 1296U},
        {"12-bit YUV420P12LE 853x480 (unaligned)", 853, 480, 12, openmedia::PixelFormat::YUV420P12LE, 1712U, 864U},
        {"12-bit YUV444P12LE 1920x1080", 1920, 1080, 12, openmedia::PixelFormat::YUV444P12LE, 3840U, 3840U},
    };

    for (const auto& c : cases) {
        verifySingleAlignmentCase(c);
    }
}

/// @brief Verifies initialize() parameter validation and supported codec acceptance.
void testInitializationValidation() {
    openmedia::CoreVideoDecoder decoder;

    openmedia::StreamInfo audioStream;
    audioStream.type = openmedia::MediaType::Audio;
    audioStream.codec = openmedia::CodecId::Opus;
    check(decoder.initialize(audioStream) == openmedia::CoreStatus::InvalidArgument,
          "TC-1.2.2 Init", "Non-video stream returns InvalidArgument");

    openmedia::StreamInfo badDimsVideo;
    badDimsVideo.type = openmedia::MediaType::Video;
    badDimsVideo.codec = openmedia::CodecId::AV1;
    badDimsVideo.width = 0;
    badDimsVideo.height = 1080;
    check(decoder.initialize(badDimsVideo) == openmedia::CoreStatus::InvalidArgument,
          "TC-1.2.2 Init", "Zero width video stream returns InvalidArgument");

    openmedia::StreamInfo unsupportedVideo;
    unsupportedVideo.type = openmedia::MediaType::Video;
    unsupportedVideo.codec = openmedia::CodecId::ProRes;
    unsupportedVideo.width = 1920;
    unsupportedVideo.height = 1080;
    check(decoder.initialize(unsupportedVideo) == openmedia::CoreStatus::UnsupportedCodec,
          "TC-1.2.2 Init", "Unsupported codec returns UnsupportedCodec");

    openmedia::StreamInfo av1Stream;
    av1Stream.type = openmedia::MediaType::Video;
    av1Stream.codec = openmedia::CodecId::AV1;
    av1Stream.width = 1920;
    av1Stream.height = 1080;
    av1Stream.bitDepth = 10;
    check(decoder.initialize(av1Stream) == openmedia::CoreStatus::Ok,
          "TC-1.2.2 Init", "AV1 stream initializes CoreVideoDecoder");

    openmedia::CoreVideoDecoder vp9Decoder;
    openmedia::StreamInfo vp9Stream;
    vp9Stream.type = openmedia::MediaType::Video;
    vp9Stream.codec = openmedia::CodecId::VP9;
    vp9Stream.width = 1920;
    vp9Stream.height = 1080;
    vp9Stream.bitDepth = 8;
    check(vp9Decoder.initialize(vp9Stream) == openmedia::CoreStatus::Ok,
          "TC-1.2.2 Init", "VP9 stream initializes CoreVideoDecoder");
}

void verifyUninitializedAndMovedGuards() {
    openmedia::CoreVideoDecoder uninitDecoder;
    openmedia::EncodedPacket pkt;
    pkt.data = {0x01, 0x02, 0x03};
    openmedia::RawVideoFrame frame;
    std::vector<openmedia::RawVideoFrame> drained;

    uninitDecoder.reset();
    check(uninitDecoder.sendPacket(pkt) == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "sendPacket before initialize returns NotInitialized");
    check(uninitDecoder.decodeNextFrame(frame) == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "decodeNextFrame before initialize returns NotInitialized");
    check(uninitDecoder.flush() == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "flush before initialize returns NotInitialized");
    check(uninitDecoder.drainBeforeFallback(drained) == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "drainBeforeFallback before initialize returns NotInitialized");

    openmedia::CoreVideoDecoder movedTo(std::move(uninitDecoder));
    openmedia::StreamInfo dummyStream;
    dummyStream.type = openmedia::MediaType::Video;
    dummyStream.codec = openmedia::CodecId::VP9;
    dummyStream.width = 640;
    dummyStream.height = 360;
    uninitDecoder.reset();
    check(uninitDecoder.initialize(dummyStream) == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "initialize on moved-from instance returns NotInitialized");
    check(uninitDecoder.sendPacket(pkt) == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "sendPacket on moved-from instance returns NotInitialized");
    check(uninitDecoder.decodeNextFrame(frame) == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "decodeNextFrame on moved-from instance returns NotInitialized");
    check(uninitDecoder.flush() == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "flush on moved-from instance returns NotInitialized");
}

bool isValidTerminalStatus(openmedia::CoreStatus status) noexcept {
    if (status == openmedia::CoreStatus::EndOfFile ||
        status == openmedia::CoreStatus::NoDecodableFrames) {
        return true;
    }
    return status == openmedia::CoreStatus::NeedMoreData ||
           status == openmedia::CoreStatus::DecodeFailed;
}

/// @brief Verifies error handling for moved-from, uninitialized, empty, and corrupt inputs.
void testErrorHandling() {
    verifyUninitializedAndMovedGuards();

    openmedia::CoreVideoDecoder decoder;
    openmedia::StreamInfo vp9Stream;
    vp9Stream.type = openmedia::MediaType::Video;
    vp9Stream.codec = openmedia::CodecId::VP9;
    vp9Stream.width = 640;
    vp9Stream.height = 360;
    if (decoder.initialize(vp9Stream) != openmedia::CoreStatus::Ok) {
        check(false, "TC-1.2.2 Error", "VP9 decoder failed to initialize for error tests");
        return;
    }

    openmedia::EncodedPacket emptyPacket;
    check(decoder.sendPacket(emptyPacket) == openmedia::CoreStatus::InvalidArgument,
          "TC-1.2.2 Error", "Empty packet returns InvalidArgument");

    openmedia::RawVideoFrame frame;
    check(decoder.decodeNextFrame(frame) == openmedia::CoreStatus::NeedMoreData,
          "TC-1.2.2 Error", "decodeNextFrame with no packets returns NeedMoreData");

    openmedia::EncodedPacket corruptPacket;
    corruptPacket.data = {0xFF, 0x00, 0xDE, 0xAD, 0xBE, 0xEF};
    const auto corruptStatus = decoder.sendPacket(corruptPacket);
    check(corruptStatus == openmedia::CoreStatus::DecodeFailed ||
              corruptStatus == openmedia::CoreStatus::Ok,
          "TC-1.2.2 Error", "Corrupt packet handled safely without crash");

    decoder.reset();
    check(decoder.flush() == openmedia::CoreStatus::Ok,
          "TC-1.2.2 Error", "flush after reset returns Ok");
    const auto drainStatus = decoder.decodeNextFrame(frame);
    check(isValidTerminalStatus(drainStatus),
          "TC-1.2.2 Error", "decodeNextFrame after flush returns terminal/need-data status");
}

openmedia::StreamInfo makeVideoStream(openmedia::CodecId codec) {
    openmedia::StreamInfo stream;
    stream.type = openmedia::MediaType::Video;
    stream.codec = codec;
    stream.width = 1280;
    stream.height = 720;
    stream.bitDepth = 8;
    return stream;
}

openmedia::EncodedPacket makePacket(int64_t pts, uint8_t marker, bool keyframe) {
    openmedia::EncodedPacket pkt;
    pkt.pts = pts;
    pkt.dts = pts;
    pkt.duration = 33;
    pkt.isKeyFrame = keyframe;
    pkt.data = {marker, 0x11, 0x22, 0x33};
    return pkt;
}

/// @brief Verifies final frame in WebM/MKV (VP9/AV1) is always emitted before EndOfFile.
void testWebmFinalFrameEofDrain() {
    openmedia::CoreVideoDecoder decoder;
    if (decoder.initialize(makeVideoStream(openmedia::CodecId::VP9)) != openmedia::CoreStatus::Ok) {
        check(false, "TC-1.2.2 EOF Drain", "Failed to initialize VP9 decoder");
        return;
    }

    check(decoder.sendPacket(makePacket(0, 0xA1, true)) == openmedia::CoreStatus::Ok,
          "TC-1.2.2 EOF Drain", "Submitted frame 0");
    check(decoder.sendPacket(makePacket(33, 0xA2, false)) == openmedia::CoreStatus::Ok,
          "TC-1.2.2 EOF Drain", "Submitted frame 1");
    check(decoder.sendPacket(makePacket(66, 0xA3, false)) == openmedia::CoreStatus::Ok,
          "TC-1.2.2 EOF Drain", "Submitted final frame 2");
    check(decoder.flush() == openmedia::CoreStatus::Ok,
          "TC-1.2.2 EOF Drain", "flush() succeeded");

    std::vector<int64_t> decodedPts;
    openmedia::RawVideoFrame frame;
    while (decoder.decodeNextFrame(frame) == openmedia::CoreStatus::Ok) {
        decodedPts.push_back(frame.pts);
    }

    const bool allThreeEmitted = decodedPts.size() == 3U &&
                                 decodedPts[0] == 0 &&
                                 decodedPts[1] == 33 &&
                                 decodedPts[2] == 66;
    check(allThreeEmitted, "TC-1.2.2 EOF Drain", "All 3 WebM frames including final frame emitted");
    check(decoder.decodeNextFrame(frame) == openmedia::CoreStatus::EndOfFile,
          "TC-1.2.2 EOF Drain", "Returns EndOfFile only after final frame is drained");
}

/// @brief Verifies non-zero start timestamps normalize so frame 0 maps to PTS = 0 without dropping.
void testZeroPtsNormalization() {
    openmedia::CoreVideoDecoder decoder;
    if (decoder.initialize(makeVideoStream(openmedia::CodecId::AV1)) != openmedia::CoreStatus::Ok) {
        check(false, "TC-1.2.2 PTS Norm", "Failed to initialize AV1 decoder");
        return;
    }

    decoder.sendPacket(makePacket(1234, 0xB1, true));
    decoder.sendPacket(makePacket(1267, 0xB2, false));
    decoder.sendPacket(makePacket(1300, 0xB3, false));
    decoder.flush();

    std::vector<int64_t> ptsList;
    openmedia::RawVideoFrame frame;
    while (decoder.decodeNextFrame(frame) == openmedia::CoreStatus::Ok) {
        ptsList.push_back(frame.pts);
    }

    const bool autoNormOk = ptsList.size() == 3U &&
                            ptsList[0] == 0 &&
                            ptsList[1] == 33 &&
                            ptsList[2] == 66;
    check(autoNormOk, "TC-1.2.2 PTS Norm", "Auto-normalized non-zero start PTS to 0 without drop");

    decoder.reset();
    decoder.resetFirstTimestamp(5000);
    decoder.sendPacket(makePacket(5000, 0xC1, true));
    decoder.sendPacket(makePacket(5033, 0xC2, false));
    decoder.flush();

    ptsList.clear();
    while (decoder.decodeNextFrame(frame) == openmedia::CoreStatus::Ok) {
        ptsList.push_back(frame.pts);
    }

    const bool explicitNormOk = ptsList.size() == 2U && ptsList[0] == 0 && ptsList[1] == 33;
    check(explicitNormOk, "TC-1.2.2 PTS Norm", "Explicit resetFirstTimestamp(5000) maps frame 0 to 0");
}

/// @brief Verifies zero decodable frames on EOF returns CoreStatus::NoDecodableFrames with diagnostic.
void testNoDecodableFramesGuard() {
    openmedia::CoreVideoDecoder decoder;
    if (decoder.initialize(makeVideoStream(openmedia::CodecId::AV1)) != openmedia::CoreStatus::Ok) {
        check(false, "TC-1.2.2 Zero-Frame", "Failed to initialize AV1 decoder");
        return;
    }

    check(decoder.flush() == openmedia::CoreStatus::Ok,
          "TC-1.2.2 Zero-Frame", "flush() on empty stream succeeds");
    openmedia::RawVideoFrame frame;
    const auto status = decoder.decodeNextFrame(frame);
    check(status == openmedia::CoreStatus::NoDecodableFrames,
          "TC-1.2.2 Zero-Frame", "Returns CoreStatus::NoDecodableFrames on 0-frame EOF");
    check(decoder.lastDiagnosticMessage() == "No decodable video frames in file",
          "TC-1.2.2 Zero-Frame", "Diagnostic message matches expected string");

    decoder.reset();
    check(decoder.lastDiagnosticMessage().empty(),
          "TC-1.2.2 Zero-Frame", "reset() clears diagnostic message");
}

void verifySingleHwFallback(openmedia::HwDecodeBackend backend, const char* label) {
    openmedia::CoreVideoDecoder decoder;
    const auto initStatus = decoder.initializeWithHardware(makeVideoStream(openmedia::CodecId::AV1), backend);
    check(initStatus == openmedia::CoreStatus::Ok, label, "initializeWithHardware succeeded");
    check(decoder.isHardwareActive() && decoder.activeBackend() == backend,
          label, "Hardware backend reported active before fallback");

    decoder.sendPacket(makePacket(1000, 0xD1, true));
    decoder.sendPacket(makePacket(1033, 0xD2, false));

    std::vector<openmedia::RawVideoFrame> drained;
    const auto fallbackStatus = decoder.drainBeforeFallback(drained);
    check(fallbackStatus == openmedia::CoreStatus::Ok, label, "drainBeforeFallback succeeded");
    check(!decoder.isHardwareActive() && decoder.activeBackend() == openmedia::HwDecodeBackend::None,
          label, "Decoder fell back to CPU (HwDecodeBackend::None)");
    check(drained.size() == 2U && drained[0].pts == 0 && drained[1].pts == 33,
          label, "Drained buffered frames with normalized PTS before CPU fallback");

    decoder.sendPacket(makePacket(1066, 0xD3, false));
    decoder.flush();
    openmedia::RawVideoFrame cpuFrame;
    check(decoder.decodeNextFrame(cpuFrame) == openmedia::CoreStatus::Ok && cpuFrame.pts == 66,
          label, "CPU fallback continues decoding with preserved PTS offset");
    check(decoder.decodeNextFrame(cpuFrame) == openmedia::CoreStatus::EndOfFile,
          label, "CPU fallback reaches EndOfFile cleanly");
}

/// @brief Verifies drainBeforeFallback() across NVDEC, D3D11VA, and VideoToolbox.
void testHardwareDrainBeforeFallback() {
    struct HwCase {
        openmedia::HwDecodeBackend backend;
        const char* label;
    };
    const std::array<HwCase, 3> cases{{
        {openmedia::HwDecodeBackend::NVDEC, "TC-1.2.2 HW NVDEC"},
        {openmedia::HwDecodeBackend::D3D11VA, "TC-1.2.2 HW D3D11VA"},
        {openmedia::HwDecodeBackend::VideoToolbox, "TC-1.2.2 HW VideoToolbox"},
    }};

    for (const auto& c : cases) {
        verifySingleHwFallback(c.backend, c.label);
    }
}

} // namespace

int main() {
    test16ByteRowAlignment();
    testInitializationValidation();
    testErrorHandling();
    testWebmFinalFrameEofDrain();
    testZeroPtsNormalization();
    testNoDecodableFramesGuard();
    testHardwareDrainBeforeFallback();

    if (gFailureCount == 0) {
        std::cout << "All TC-1.2.2 CoreVideoDecoder tests passed.\n";
        return 0;
    }
    std::cerr << gFailureCount << " TC-1.2.2 CoreVideoDecoder test(s) failed.\n";
    return 1;
}
