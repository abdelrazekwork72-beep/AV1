#include "importer/pixel_format_8u.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace openmedia::importer;

namespace {

void testOpaqueFormatNegotiation() {
    csSDK_uint32 fmt = 0;
    assert(PixelFormat8uEngine::getSupported8uFormat(0, false, fmt) == kImNoErr);
    assert(fmt == kPixFmtYUV420Planar_8u_709);

    assert(PixelFormat8uEngine::getSupported8uFormat(1, false, fmt) == kImNoErr);
    assert(fmt == kPixFmtBGRA4444_8u);

    assert(PixelFormat8uEngine::getSupported8uFormat(2, false, fmt) == kImNoErr);
    assert(fmt == kPixFmtVUYA4444_8u);

    assert(PixelFormat8uEngine::getSupported8uFormat(3, false, fmt) == kImBadFormatIndex);
    assert(PixelFormat8uEngine::getSupported8uFormat(-1, false, fmt) == kImBadFormatIndex);
}

void testVp9WebmAlphaNegotiation() {
    csSDK_uint32 fmt = 0;
    assert(PixelFormat8uEngine::getSupported8uFormat(0, true, fmt) == kImNoErr);
    assert(fmt == kPixFmtBGRA4444_8u);

    assert(PixelFormat8uEngine::getSupported8uFormat(1, true, fmt) == kImNoErr);
    assert(fmt == kPixFmtVUYA4444_8u);

    assert(PixelFormat8uEngine::getSupported8uFormat(2, true, fmt) == kImBadFormatIndex);
}

void testYuva420pAlphaPreservation() {
    constexpr int32_t kW = 4;
    constexpr int32_t kH = 2;
    constexpr int32_t kDstStride = 32; // 16-byte aligned padded stride (> 4 * 4 = 16)

    const std::vector<uint8_t> yPlane = {
        235, 235, 16, 16,
        81, 81, 145, 145,
    };
    const std::vector<uint8_t> uPlane = {128, 128};
    const std::vector<uint8_t> vPlane = {128, 128};
    const std::vector<uint8_t> aPlane = {
        0, 64, 128, 255,
        17, 99, 200, 254,
    };

    Yuva420pPlaneView view{};
    view.yPlane = yPlane.data();
    view.uPlane = uPlane.data();
    view.vPlane = vPlane.data();
    view.aPlane = aPlane.data();
    view.yStride = kW;
    view.uStride = kW / 2;
    view.vStride = kW / 2;
    view.aStride = kW;
    view.width = kW;
    view.height = kH;

    std::vector<uint8_t> dst(static_cast<size_t>(kDstStride * kH), 0xAAU);
    assert(PixelFormat8uEngine::packYuva420pToBgra8(view, dst.data(), kDstStride) == kImNoErr);

    for (int32_t r = 0; r < kH; ++r) {
        for (int32_t c = 0; c < kW; ++c) {
            const size_t idx = static_cast<size_t>(r * kDstStride + c * 4);
            const uint8_t expectedAlpha = aPlane[static_cast<size_t>(r * kW + c)];
            assert(dst[idx + 3] == expectedAlpha);
        }
    }

    // Verify white (Y=235) and black (Y=16) luma endpoints
    assert(dst[0] == 255 && dst[1] == 255 && dst[2] == 255);
    assert(dst[8] == 0 && dst[9] == 0 && dst[10] == 0);
}

void testOpaqueFallbackAndInvalidInputs() {
    constexpr int32_t kW = 2;
    constexpr int32_t kH = 2;
    const std::vector<uint8_t> yPlane = {128, 128, 128, 128};
    const std::vector<uint8_t> uPlane = {128};
    const std::vector<uint8_t> vPlane = {128};

    Yuva420pPlaneView view{};
    view.yPlane = yPlane.data();
    view.uPlane = uPlane.data();
    view.vPlane = vPlane.data();
    view.aPlane = nullptr;
    view.yStride = kW;
    view.uStride = 1;
    view.vStride = 1;
    view.width = kW;
    view.height = kH;

    std::vector<uint8_t> dst(static_cast<size_t>(kW * 4 * kH), 0U);
    assert(PixelFormat8uEngine::packYuva420pToBgra8(view, dst.data(), kW * 4) == kImNoErr);
    for (int32_t i = 0; i < kW * kH; ++i) {
        assert(dst[static_cast<size_t>(i * 4 + 3)] == 255U);
    }

    assert(PixelFormat8uEngine::packYuva420pToBgra8(view, nullptr, kW * 4) == kImMemErr);
    assert(PixelFormat8uEngine::packYuva420pToBgra8(view, dst.data(), 4) == kImMemErr);
}

} // namespace

int main() {
    testOpaqueFormatNegotiation();
    testVp9WebmAlphaNegotiation();
    testYuva420pAlphaPreservation();
    testOpaqueFallbackAndInvalidInputs();
    std::cout << "[PASS] TC-2.2.2 PixelFormat8uEngine 8-bit & WebM YUVA420P alpha tests\n";
    return 0;
}
