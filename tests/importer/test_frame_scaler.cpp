/// @file test_frame_scaler.cpp
/// @brief Standalone C++17 unit tests (TC-2.2.5) for FrameScaler context caching and color matrices.

#include "importer/frame_scaler.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace {

using openmedia::ColorRange;
using openmedia::ColorSpace;
using openmedia::CoreStatus;
using openmedia::PixelFormat;
using openmedia::RawVideoFrame;
using openmedia::importer::ColorMatrix;
using openmedia::importer::FrameScaler;
using openmedia::importer::ScaleConfig;

int gFailureCount = 0;

void check(bool condition, const char* testName, const char* detail) {
    if (condition) {
        std::cout << "[PASS] " << testName << ": " << detail << "\n";
        return;
    }
    std::cerr << "[FAIL] " << testName << ": " << detail << "\n";
    ++gFailureCount;
}

RawVideoFrame makeYuv420Frame(int32_t width,
                              int32_t height,
                              ColorSpace space,
                              uint8_t yVal,
                              uint8_t uVal,
                              uint8_t vVal) {
    RawVideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.bitDepth = 8;
    frame.format = PixelFormat::YUV420P;
    frame.colorSpace = space;
    frame.colorRange = ColorRange::Limited;

    const uint32_t yStride = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width));
    const uint32_t uvWidth = static_cast<uint32_t>((width + 1) / 2);
    const uint32_t uvStride = RawVideoFrame::alignRowStride16(uvWidth);
    const uint32_t uvHeight = static_cast<uint32_t>((height + 1) / 2);

    frame.rowBytes[0] = yStride;
    frame.rowBytes[1] = uvStride;
    frame.rowBytes[2] = uvStride;

    frame.planes[0].assign(static_cast<size_t>(yStride) * height, yVal);
    frame.planes[1].assign(static_cast<size_t>(uvStride) * uvHeight, uVal);
    frame.planes[2].assign(static_cast<size_t>(uvStride) * uvHeight, vVal);
    return frame;
}

void testConsecutiveFrameContextReuse() {
    FrameScaler scaler;
    const int32_t width = 64;
    const int32_t height = 32;
    const int32_t dstStride = static_cast<int32_t>(
        RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width * 4)));

    RawVideoFrame src = makeYuv420Frame(width, height, ColorSpace::BT709, 128, 128, 128);
    std::vector<uint8_t> dst(static_cast<size_t>(dstStride) * height, 0);

    for (int i = 0; i < 100; ++i) {
        const CoreStatus status =
            scaler.scaleFrame(src, dst.data(), dstStride, PixelFormat::BGRA8);
        if (status != CoreStatus::Ok) {
            check(false, "TC-2.2.5 Reuse", "scaleFrame failed during 100-frame loop");
            return;
        }
    }

    check(scaler.scaleCount() == 100U,
          "TC-2.2.5 Reuse",
          "100 consecutive frames scaled");
    check(scaler.cacheMissCount() == 1U,
          "TC-2.2.5 Reuse",
          "100 consecutive identical frames triggered only 1 SwsContext allocation");

    RawVideoFrame resized = makeYuv420Frame(128, 64, ColorSpace::BT709, 128, 128, 128);
    const int32_t resizedStride = static_cast<int32_t>(
        RawVideoFrame::alignRowStride16(128U * 4U));
    std::vector<uint8_t> resizedDst(static_cast<size_t>(resizedStride) * 64, 0);
    const CoreStatus resizeStatus =
        scaler.scaleFrame(resized, resizedDst.data(), resizedStride, PixelFormat::BGRA8);

    check(resizeStatus == CoreStatus::Ok && scaler.cacheMissCount() == 2U,
          "TC-2.2.5 Reuse",
          "Dimension change triggers exactly 1 new SwsContext allocation");
}

void testColorMatrices() {
    check(FrameScaler::mapColorSpace(ColorSpace::BT601) == ColorMatrix::Rec601,
          "TC-2.2.5 Matrix",
          "BT601 maps to ColorMatrix::Rec601");
    check(FrameScaler::mapColorSpace(ColorSpace::BT709) == ColorMatrix::Rec709,
          "TC-2.2.5 Matrix",
          "BT709 maps to ColorMatrix::Rec709");
    check(FrameScaler::mapColorSpace(ColorSpace::BT2020_NCL) == ColorMatrix::Rec2020 &&
          FrameScaler::mapColorSpace(ColorSpace::BT2020_CL) == ColorMatrix::Rec2020,
          "TC-2.2.5 Matrix",
          "BT2020 NCL/CL map to ColorMatrix::Rec2020");

    check(FrameScaler::toSwsColorspaceId(ColorMatrix::Rec601) == 5 &&
          FrameScaler::toSwsColorspaceId(ColorMatrix::Rec709) == 1 &&
          FrameScaler::toSwsColorspaceId(ColorMatrix::Rec2020) == 9,
          "TC-2.2.5 Matrix",
          "Sws colorspace IDs match ITU-R BT.601/709/2020");

    const auto c601 = FrameScaler::getMatrixCoefficients(ColorMatrix::Rec601);
    const auto c709 = FrameScaler::getMatrixCoefficients(ColorMatrix::Rec709);
    const auto c2020 = FrameScaler::getMatrixCoefficients(ColorMatrix::Rec2020);
    check(c601 != c709 && c709 != c2020 && c601 != c2020,
          "TC-2.2.5 Matrix",
          "Rec.601, Rec.709, and Rec.2020 have distinct matrix coefficients");

    FrameScaler scaler;
    const int32_t width = 16;
    const int32_t height = 16;
    const int32_t stride = width * 4;
    std::vector<uint8_t> out601(static_cast<size_t>(stride) * height, 0);
    std::vector<uint8_t> out709(static_cast<size_t>(stride) * height, 0);
    std::vector<uint8_t> out2020(static_cast<size_t>(stride) * height, 0);

    const RawVideoFrame f601 = makeYuv420Frame(width, height, ColorSpace::BT601, 140, 90, 200);
    const RawVideoFrame f709 = makeYuv420Frame(width, height, ColorSpace::BT709, 140, 90, 200);
    const RawVideoFrame f2020 = makeYuv420Frame(width, height, ColorSpace::BT2020_NCL, 140, 90, 200);

    scaler.scaleFrame(f601, out601.data(), stride, PixelFormat::BGRA8);
    check(scaler.activeColorMatrix() == ColorMatrix::Rec601,
          "TC-2.2.5 Matrix",
          "Active matrix updated to Rec601");

    scaler.scaleFrame(f709, out709.data(), stride, PixelFormat::BGRA8);
    check(scaler.activeColorMatrix() == ColorMatrix::Rec709,
          "TC-2.2.5 Matrix",
          "Active matrix updated to Rec709");

    scaler.scaleFrame(f2020, out2020.data(), stride, PixelFormat::BGRA8);
    check(scaler.activeColorMatrix() == ColorMatrix::Rec2020,
          "TC-2.2.5 Matrix",
          "Active matrix updated to Rec2020");

    check(out601 != out709 && out709 != out2020,
          "TC-2.2.5 Matrix",
          "Rec.601, Rec.709, and Rec.2020 produce distinct RGB conversion output");
}

void testDirectDstStrideAndSafety() {
    FrameScaler scaler;
    const int32_t oddWidth = 1921;
    const int32_t height = 4;
    const int32_t dstStride = static_cast<int32_t>(
        RawVideoFrame::alignRowStride16(static_cast<uint32_t>(oddWidth * 4)));
    check(dstStride == 7696,
          "TC-2.2.5 Stride",
          "1921px BGRA8 aligns to 7696-byte row stride");

    const RawVideoFrame src = makeYuv420Frame(oddWidth, height, ColorSpace::BT709, 180, 128, 128);
    constexpr uint8_t kSentinel = 0xA5;
    std::vector<uint8_t> dst(static_cast<size_t>(dstStride) * height + 64, kSentinel);

    const CoreStatus status =
        scaler.scaleFrame(src, dst.data(), dstStride, PixelFormat::BGRA8);
    check(status == CoreStatus::Ok,
          "TC-2.2.5 Stride",
          "Odd-width 1921x4 frame scales cleanly with direct dstStride");

    bool sentinelIntact = true;
    for (size_t i = static_cast<size_t>(dstStride) * height; i < dst.size(); ++i) {
        if (dst[i] != kSentinel) {
            sentinelIntact = false;
            break;
        }
    }
    check(sentinelIntact,
          "TC-2.2.5 Stride",
          "Trailing sentinel bytes untouched (zero buffer overrun)");

    ScaleConfig invalidCfg;
    const uint8_t* nullPlanes[4] = {nullptr, nullptr, nullptr, nullptr};
    const int32_t zeroStrides[4] = {0, 0, 0, 0};
    uint8_t* dstPlanes[4] = {dst.data(), nullptr, nullptr, nullptr};
    const int32_t dstStrides[4] = {dstStride, 0, 0, 0};
    check(scaler.scale(invalidCfg, nullPlanes, zeroStrides, dstPlanes, dstStrides) ==
              CoreStatus::InvalidArgument,
          "TC-2.2.5 Errors",
          "Invalid config/null planes returns InvalidArgument");

    scaler.reset();
    check(scaler.cacheMissCount() == 0U && scaler.scaleCount() == 0U,
          "TC-2.2.5 Reset",
          "reset() clears cache miss and scale counters");
}

} // namespace

int main() {
    testConsecutiveFrameContextReuse();
    testColorMatrices();
    testDirectDstStrideAndSafety();

    if (gFailureCount == 0) {
        std::cout << "All TC-2.2.5 FrameScaler tests passed.\n";
        return 0;
    }
    std::cerr << gFailureCount << " TC-2.2.5 FrameScaler test(s) failed.\n";
    return 1;
}
