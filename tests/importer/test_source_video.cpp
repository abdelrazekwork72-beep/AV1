#include "importer/source_video.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using openmedia::CoreStatus;
using openmedia::EncodedPacket;
using openmedia::ICoreVideoDecoder;
using openmedia::PixelFormat;
using openmedia::RawVideoFrame;
using openmedia::StreamInfo;
using openmedia::importer::ImSourceVideoRecExt;
using openmedia::importer::SourceVideoHandler;
using openmedia::importer::kImMemErr;
using openmedia::importer::kImNoErr;
using openmedia::importer::kImUnsupported;
using openmedia::importer::kPPixBufferAccessWriteOnly;
using openmedia::importer::kPixFmtBGRA4444_8u;

struct MockPPixBuffer {
    int32_t width = 0;
    int32_t height = 0;
    csSDK_int32 rowBytes = 0;
    PrPixelFormat format = PrPixelFormat_Invalid;
    std::vector<char> pixels;
};

struct MockPPixTracker {
    int createCalls = 0;
    int disposeCalls = 0;
    int getPixelsCalls = 0;
    int getRowBytesCalls = 0;
    csSDK_int32 lastAccess = 0;
    bool failCreate = false;
    bool failGetPixels = false;
    bool failGetRowBytes = false;
    csSDK_int32 forcedRowBytes = 0;
};

MockPPixTracker g_tracker;

csSDK_int32 align16(csSDK_int32 value) {
    return ((value + 15) / 16) * 16;
}

prSuiteError mockCreatePPix(PPixHand* outPPix,
                            csSDK_int32 access,
                            PrPixelFormat format,
                            const prRect* bounds) {
    ++g_tracker.createCalls;
    g_tracker.lastAccess = access;
    if (g_tracker.failCreate || outPPix == nullptr || bounds == nullptr) {
        return suiteError_OutOfMemory;
    }
    const int32_t w = bounds->right - bounds->left;
    const int32_t h = bounds->bottom - bounds->top;
    const csSDK_int32 stride = (g_tracker.forcedRowBytes > 0)
                                   ? g_tracker.forcedRowBytes
                                   : align16(w * 4);
    auto* buf = new MockPPixBuffer();
    buf->width = w;
    buf->height = h;
    buf->rowBytes = stride;
    buf->format = format;
    buf->pixels.assign(static_cast<std::size_t>(stride) * static_cast<std::size_t>(h), 0);
    *outPPix = static_cast<PPixHand>(buf);
    return suiteError_NoError;
}

prSuiteError mockDispose(PPixHand ppix) {
    ++g_tracker.disposeCalls;
    delete static_cast<MockPPixBuffer*>(ppix);
    return suiteError_NoError;
}

prSuiteError mockGetPixels(PPixHand ppix, csSDK_int32 requestedAccess, char** outPixels) {
    ++g_tracker.getPixelsCalls;
    if (g_tracker.failGetPixels || ppix == nullptr || outPixels == nullptr ||
        requestedAccess != kPPixBufferAccessWriteOnly) {
        return suiteError_Fail;
    }
    auto* buf = static_cast<MockPPixBuffer*>(ppix);
    *outPixels = buf->pixels.data();
    return suiteError_NoError;
}

prSuiteError mockGetRowBytes(PPixHand ppix, csSDK_int32* outRowBytes) {
    ++g_tracker.getRowBytesCalls;
    if (g_tracker.failGetRowBytes || ppix == nullptr || outRowBytes == nullptr) {
        return suiteError_Fail;
    }
    auto* buf = static_cast<MockPPixBuffer*>(ppix);
    *outRowBytes = buf->rowBytes;
    return suiteError_NoError;
}

PrSDKPPixSuite makeMockPPixSuite() {
    PrSDKPPixSuite suite{};
    suite.Dispose = &mockDispose;
    suite.GetPixels = &mockGetPixels;
    suite.GetRowBytes = &mockGetRowBytes;
    return suite;
}

class MockVideoDecoder final : public ICoreVideoDecoder {
public:
    CoreStatus nextStatus = CoreStatus::Ok;
    RawVideoFrame frameToReturn;

    CoreStatus initialize(const StreamInfo&) override { return CoreStatus::Ok; }
    CoreStatus sendPacket(const EncodedPacket&) override { return CoreStatus::Ok; }
    CoreStatus decodeNextFrame(RawVideoFrame& outFrame) override {
        if (nextStatus != CoreStatus::Ok) {
            return nextStatus;
        }
        outFrame = frameToReturn;
        return CoreStatus::Ok;
    }
    CoreStatus flush() override { return CoreStatus::Ok; }
    void reset() override {}
};

RawVideoFrame makeBgraFrame(int32_t width, int32_t height, uint8_t seed) {
    RawVideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.bitDepth = 8;
    frame.format = PixelFormat::BGRA8;
    frame.rowBytes[0] = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width) * 4U);
    frame.planes[0].resize(static_cast<std::size_t>(frame.rowBytes[0]) * static_cast<std::size_t>(height));
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width * 4; ++x) {
            const auto idx = static_cast<std::size_t>(y) * frame.rowBytes[0] + static_cast<std::size_t>(x);
            frame.planes[0][idx] = static_cast<uint8_t>((seed + y + x) & 0xFFU);
        }
    }
    return frame;
}

void test1080pSourceVideoSuccess() {
    g_tracker = MockPPixTracker{};
    const PrSDKPPixSuite ppixSuite = makeMockPPixSuite();
    const PrSDKPPixCreatorSuite creatorSuite{&mockCreatePPix, nullptr};

    MockVideoDecoder decoder;
    decoder.frameToReturn = makeBgraFrame(1920, 1080, 0x2A);

    PPixHand outFrame = nullptr;
    ImSourceVideoRecExt rec{};
    rec.inFrame = 42;
    rec.inPixelFormat = kPixFmtBGRA4444_8u;
    rec.outFrame = &outFrame;
    rec.ppixSuite = &ppixSuite;

    const csSDK_int32 rc = SourceVideoHandler::renderSourceVideo(rec, decoder, &creatorSuite);
    assert(rc == kImNoErr);
    assert(outFrame != nullptr);
    assert(g_tracker.createCalls == 1);
    assert(g_tracker.getPixelsCalls == 1);
    assert(g_tracker.getRowBytesCalls == 1);
    assert(g_tracker.disposeCalls == 0);

    auto* buf = static_cast<MockPPixBuffer*>(outFrame);
    assert(buf->width == 1920);
    assert(buf->height == 1080);
    assert(buf->rowBytes == 1920 * 4);
    assert(static_cast<uint8_t>(buf->pixels[0]) == 0x2A);
    assert(static_cast<uint8_t>(buf->pixels[static_cast<std::size_t>(buf->rowBytes) * 1079 + 1919 * 4 + 3]) ==
           decoder.frameToReturn.planes[0][static_cast<std::size_t>(decoder.frameToReturn.rowBytes[0]) * 1079 + 1919 * 4 + 3]);

    mockDispose(outFrame);
}

void testOddWidth16ByteStrideAlignment() {
    g_tracker = MockPPixTracker{};
    const PrSDKPPixSuite ppixSuite = makeMockPPixSuite();
    MockVideoDecoder decoder;
    decoder.frameToReturn = makeBgraFrame(1921, 1080, 0x11);

    PPixHand outFrame = nullptr;
    ImSourceVideoRecExt rec{};
    rec.inPixelFormat = kPixFmtBGRA4444_8u;
    rec.outFrame = &outFrame;
    rec.ppixSuite = &ppixSuite;

    imStdParms stdParms{};
    const csSDK_int32 rc = SourceVideoHandler::handleGetSourceVideo(&stdParms, &rec, &decoder, &mockCreatePPix);
    assert(rc == kImNoErr);
    assert(outFrame != nullptr);

    auto* buf = static_cast<MockPPixBuffer*>(outFrame);
    assert(buf->rowBytes == 7696);
    assert(buf->rowBytes % 16 == 0);
    assert(static_cast<uint8_t>(buf->pixels[static_cast<std::size_t>(buf->rowBytes) * 500 + 100]) ==
           decoder.frameToReturn.planes[0][static_cast<std::size_t>(decoder.frameToReturn.rowBytes[0]) * 500 + 100]);

    mockDispose(outFrame);
}

void testErrorHandlingAndCleanup() {
    const PrSDKPPixSuite ppixSuite = makeMockPPixSuite();
    MockVideoDecoder decoder;
    decoder.frameToReturn = makeBgraFrame(1920, 1080, 0x05);

    // 1. Allocation failure -> kImMemErr
    g_tracker = MockPPixTracker{};
    g_tracker.failCreate = true;
    PPixHand outFrame = nullptr;
    ImSourceVideoRecExt rec{};
    rec.outFrame = &outFrame;
    rec.ppixSuite = &ppixSuite;
    assert(SourceVideoHandler::renderSourceVideo(rec, decoder, &mockCreatePPix) == kImMemErr);
    assert(outFrame == nullptr);

    // 2. GetPixels failure -> disposes PPix and returns kImUnsupported
    g_tracker = MockPPixTracker{};
    g_tracker.failGetPixels = true;
    assert(SourceVideoHandler::renderSourceVideo(rec, decoder, &mockCreatePPix) == kImUnsupported);
    assert(g_tracker.disposeCalls == 1);
    assert(outFrame == nullptr);

    // 3. Oversized dimensions (TM-2.2.2) -> kImUnsupported without CreatePPix
    g_tracker = MockPPixTracker{};
    decoder.frameToReturn.width = 65536;
    decoder.frameToReturn.height = 65536;
    assert(SourceVideoHandler::renderSourceVideo(rec, decoder, &mockCreatePPix) == kImUnsupported);
    assert(g_tracker.createCalls == 0);

    // 4. Decoder failure -> kImUnsupported
    decoder.nextStatus = CoreStatus::DecodeFailed;
    assert(SourceVideoHandler::renderSourceVideo(rec, decoder, &mockCreatePPix) == kImUnsupported);

    // 5. Null arguments -> kImUnsupported
    assert(SourceVideoHandler::handleGetSourceVideo(nullptr, nullptr, &decoder, &mockCreatePPix) == kImUnsupported);
}

} // namespace

int main() {
    test1080pSourceVideoSuccess();
    testOddWidth16ByteStrideAlignment();
    testErrorHandlingAndCleanup();
    std::cout << "[PASS] TC-2.2.1 SourceVideoHandler unit tests passed." << std::endl;
    return 0;
}
