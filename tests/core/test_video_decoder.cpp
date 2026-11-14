/// @file test_video_decoder.cpp
/// @brief Standalone unit tests (TC-1.2.2) for CoreVideoDecoder.

#include "openmedia/video_decoder.hpp"

#include <cstdint>
#include <iostream>
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

/// @brief Verifies RawVideoFrame::alignRowStride16 and 16-byte row alignment across 8/10/12-bit formats.
void test16ByteRowAlignment() {
    struct AlignmentCase {
        const char* label;
        int32_t width;
        int32_t height;
        int32_t bitDepth;
        openmedia::PixelFormat format;
        uint32_t expectedYStride;
        uint32_t expectedUVStride;
    };

    const std::vector<AlignmentCase> cases = {
        {"8-bit YUV420P 1920x1080", 1920, 1080, 8, openmedia::PixelFormat::YUV420P, 1920U, 960U},
        {"8-bit YUV420P 1919x1080 (odd)", 1919, 1080, 8, openmedia::PixelFormat::YUV420P, 1920U, 960U},
        {"10-bit YUV420P10LE 1920x1080", 1920, 1080, 10, openmedia::PixelFormat::YUV420P10LE, 3840U, 1920U},
        {"10-bit YUV420P10LE 1281x720 (unaligned)", 1281, 720, 10, openmedia::PixelFormat::YUV420P10LE, 2576U, 1296U},
        {"12-bit YUV420P12LE 853x480 (unaligned)", 853, 480, 12, openmedia::PixelFormat::YUV420P12LE, 1712U, 864U},
        {"12-bit YUV444P12LE 1920x1080", 1920, 1080, 12, openmedia::PixelFormat::YUV444P12LE, 3840U, 3840U},
    };

    for (const auto& c : cases) {
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

        const bool mod16Ok = (frame.rowBytes[0] % 16U == 0U) &&
                             (frame.rowBytes[1] % 16U == 0U) &&
                             (frame.rowBytes[2] % 16U == 0U);
        const bool expectedOk = (frame.rowBytes[0] == c.expectedYStride) &&
                                (frame.rowBytes[1] == c.expectedUVStride) &&
                                (frame.rowBytes[2] == c.expectedUVStride);
        check(mod16Ok && expectedOk, "TC-1.2.2 Alignment", c.label);
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
    const auto av1Status = decoder.initialize(av1Stream);
    check(av1Status == openmedia::CoreStatus::Ok,
          "TC-1.2.2 Init", "AV1 stream initializes CoreVideoDecoder");

    openmedia::CoreVideoDecoder vp9Decoder;
    openmedia::StreamInfo vp9Stream;
    vp9Stream.type = openmedia::MediaType::Video;
    vp9Stream.codec = openmedia::CodecId::VP9;
    vp9Stream.width = 1920;
    vp9Stream.height = 1080;
    vp9Stream.bitDepth = 8;
    const auto vp9Status = vp9Decoder.initialize(vp9Stream);
    check(vp9Status == openmedia::CoreStatus::Ok,
          "TC-1.2.2 Init", "VP9 stream initializes CoreVideoDecoder");
}

/// @brief Verifies error handling for uninitialized calls, empty packets, and corrupt payloads.
void testErrorHandling() {
    openmedia::CoreVideoDecoder uninitDecoder;
    openmedia::EncodedPacket pkt;
    pkt.data = {0x01, 0x02, 0x03};
    openmedia::RawVideoFrame frame;

    uninitDecoder.reset();
    check(uninitDecoder.sendPacket(pkt) == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "sendPacket before initialize returns NotInitialized");
    check(uninitDecoder.decodeNextFrame(frame) == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "decodeNextFrame before initialize returns NotInitialized");
    check(uninitDecoder.flush() == openmedia::CoreStatus::NotInitialized,
          "TC-1.2.2 Error", "flush before initialize returns NotInitialized");

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
    check(drainStatus == openmedia::CoreStatus::EndOfFile ||
              drainStatus == openmedia::CoreStatus::NeedMoreData ||
              drainStatus == openmedia::CoreStatus::DecodeFailed,
          "TC-1.2.2 Error", "decodeNextFrame after flush returns terminal/need-data status");
}

} // namespace

int main() {
    test16ByteRowAlignment();
    testInitializationValidation();
    testErrorHandling();

    if (gFailureCount == 0) {
        std::cout << "All TC-1.2.2 CoreVideoDecoder tests passed.\n";
        return 0;
    }
    std::cerr << gFailureCount << " TC-1.2.2 CoreVideoDecoder test(s) failed.\n";
    return 1;
}
