/// @file test_pixel_format_hdr.cpp
/// @brief Standalone C++17 unit tests (TC-2.2.3) for PixelFormatHdrEngine.

#include "importer/pixel_format_hdr.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using openmedia::importer::HdrFormatDescriptor;
using openmedia::importer::HdrStreamInfo;
using openmedia::importer::ImIndPixelFormatRecExt;
using openmedia::importer::PixelFormatHdrEngine;
using openmedia::importer::kImBadFormatIndex;
using openmedia::importer::kImNoErr;
using openmedia::importer::kImUnsupported;
using openmedia::importer::kPixFmtBGRA4444_32f_Linear;
using openmedia::importer::kPixFmtRGB444_10u;
using openmedia::importer::kPixFmtUYVY422_32f;

int gFailureCount = 0;

void check(bool condition, const char* testName, const char* detail) {
    if (condition) {
        std::cout << "[PASS] " << testName << ": " << detail << "\n";
        return;
    }
    std::cerr << "[FAIL] " << testName << ": " << detail << "\n";
    ++gFailureCount;
}

void testHdrFormatNegotiation(std::uint8_t bitDepth, const char* label) {
    HdrStreamInfo info{};
    info.bitDepth = bitDepth;
    info.preferLinearFloat = true;

    const csSDK_uint32 expected[PixelFormatHdrEngine::kHdrFormatCount] = {
        kPixFmtRGB444_10u,
        kPixFmtUYVY422_32f,
        kPixFmtBGRA4444_32f_Linear,
    };

    for (csSDK_int32 i = 0; i < static_cast<csSDK_int32>(PixelFormatHdrEngine::kHdrFormatCount); ++i) {
        ImIndPixelFormatRecExt rec{};
        rec.index = i;
        const csSDK_int32 rc = PixelFormatHdrEngine::negotiateFormat(rec, info);
        check(rc == kImNoErr && rec.outPixelFormat == expected[i],
              label,
              "negotiated expected HDR pixel format");
        check(PixelFormatHdrEngine::isSupportedHdrFormat(rec.outPixelFormat),
              label,
              "format recognized as supported HDR format");
    }

    ImIndPixelFormatRecExt oobRec{};
    oobRec.index = static_cast<csSDK_int32>(PixelFormatHdrEngine::kHdrFormatCount);
    check(PixelFormatHdrEngine::negotiateFormat(oobRec, info) == kImBadFormatIndex,
          label,
          "out-of-range index returns kImBadFormatIndex");

    ImIndPixelFormatRecExt negRec{};
    negRec.index = -1;
    check(PixelFormatHdrEngine::negotiateFormat(negRec, info) == kImBadFormatIndex,
          label,
          "negative index returns kImBadFormatIndex");
}

void testUnsupported8BitStream() {
    HdrStreamInfo info8{};
    info8.bitDepth = 8U;

    ImIndPixelFormatRecExt rec{};
    rec.index = 0;
    check(PixelFormatHdrEngine::negotiateFormat(rec, info8) == kImUnsupported,
          "TC-2.2.3-8bit-reject",
          "8-bit stream rejected by PixelFormatHdrEngine");
}

void testFormatDescriptors() {
    const HdrFormatDescriptor* d0 = PixelFormatHdrEngine::getFormatDescriptor(0U);
    const HdrFormatDescriptor* d1 = PixelFormatHdrEngine::getFormatDescriptor(1U);
    const HdrFormatDescriptor* d2 = PixelFormatHdrEngine::getFormatDescriptor(2U);
    const HdrFormatDescriptor* d3 = PixelFormatHdrEngine::getFormatDescriptor(3U);

    check(d0 != nullptr && d0->pixelFormat == kPixFmtRGB444_10u && !d0->isLinearFloat,
          "TC-2.2.3-desc-0",
          "descriptor 0 is RGB_444_10u");
    check(d1 != nullptr && d1->pixelFormat == kPixFmtUYVY422_32f && d1->isLinearFloat,
          "TC-2.2.3-desc-1",
          "descriptor 1 is UYVY_422_32f");
    check(d2 != nullptr && d2->pixelFormat == kPixFmtBGRA4444_32f_Linear && d2->isLinearFloat,
          "TC-2.2.3-desc-2",
          "descriptor 2 is BGRA_4444_32f_Linear");
    check(d3 == nullptr, "TC-2.2.3-desc-oob", "out-of-bounds descriptor returns nullptr");
}

void test10BitPrecisionPreservation() {
    // 512 and 513 both truncate to 128 in 8-bit (>> 2), so they must remain distinct in 32f.
    const float f0 = PixelFormatHdrEngine::normalizeSampleToFloat(512U, 10U);
    const float f1 = PixelFormatHdrEngine::normalizeSampleToFloat(513U, 10U);
    check(f1 > f0 && (f1 - f0) > 1e-4f,
          "TC-2.2.3-10bit-precision",
          "adjacent 10-bit samples remain strictly distinct (> 8-bit precision)");

    const float fMin = PixelFormatHdrEngine::normalizeSampleToFloat(0U, 10U);
    const float fMax = PixelFormatHdrEngine::normalizeSampleToFloat(1023U, 10U);
    const float fClamp = PixelFormatHdrEngine::normalizeSampleToFloat(2000U, 10U);
    check(fMin == 0.0f && std::fabs(fMax - 1.0f) < 1e-6f && std::fabs(fClamp - 1.0f) < 1e-6f,
          "TC-2.2.3-10bit-bounds",
          "10-bit normalization bounds [0.0f, 1.0f] and clamping");
}

void test12BitPrecisionPreservation() {
    // 2048 and 2049 both truncate to 128 in 8-bit (>> 4), so they must remain distinct in 32f.
    const float f0 = PixelFormatHdrEngine::normalizeSampleToFloat(2048U, 12U);
    const float f1 = PixelFormatHdrEngine::normalizeSampleToFloat(2049U, 12U);
    check(f1 > f0 && (f1 - f0) > 1e-4f,
          "TC-2.2.3-12bit-precision",
          "adjacent 12-bit samples remain strictly distinct (> 8-bit precision)");

    const float fMax = PixelFormatHdrEngine::normalizeSampleToFloat(4095U, 12U);
    check(std::fabs(fMax - 1.0f) < 1e-6f,
          "TC-2.2.3-12bit-max",
          "12-bit max sample normalizes to 1.0f");
}

void testPlanarYuvToBgra32fConversion() {
    // Neutral mid-gray at adjacent 12-bit luma values (2048 vs 2049) with neutral chroma (2048).
    const std::vector<std::uint16_t> yPlane = {2048U, 2049U};
    const std::vector<std::uint16_t> uPlane = {2048U, 2048U};
    const std::vector<std::uint16_t> vPlane = {2048U, 2048U};
    std::vector<float> dst(8U, 0.0f);

    const bool ok = PixelFormatHdrEngine::convertYuvPlanesToBgra32f(
        yPlane.data(), uPlane.data(), vPlane.data(), 2U, 12U, dst.data(), dst.size());
    check(ok, "TC-2.2.3-planar-convert", "12-bit YUV planes convert to BGRA_4444_32f_Linear");

    // Verify alpha is 1.0f and pixel 1 > pixel 0 in R/G/B (no 8-bit truncation).
    check(dst[3] == 1.0f && dst[7] == 1.0f,
          "TC-2.2.3-planar-alpha",
          "BGRA32f alpha channel initialized to 1.0f");
    check(dst[4] > dst[0] && dst[5] > dst[1] && dst[6] > dst[2],
          "TC-2.2.3-planar-12bit-delta",
          "adjacent 12-bit YUV pixels produce distinct BGRA32f values");

    // Verify invalid buffer / bit depth guards.
    check(!PixelFormatHdrEngine::convertYuvPlanesToBgra32f(
              nullptr, uPlane.data(), vPlane.data(), 2U, 12U, dst.data(), dst.size()),
          "TC-2.2.3-null-guard",
          "null plane pointer rejected");
    check(!PixelFormatHdrEngine::convertYuvPlanesToBgra32f(
              yPlane.data(), uPlane.data(), vPlane.data(), 2U, 12U, dst.data(), 4U),
          "TC-2.2.3-size-guard",
          "undersized destination buffer rejected");
}

} // namespace

int main() {
    testHdrFormatNegotiation(10U, "TC-2.2.3-10bit-negotiation");
    testHdrFormatNegotiation(12U, "TC-2.2.3-12bit-negotiation");
    testUnsupported8BitStream();
    testFormatDescriptors();
    test10BitPrecisionPreservation();
    test12BitPrecisionPreservation();
    testPlanarYuvToBgra32fConversion();

    if (gFailureCount == 0) {
        std::cout << "ALL TC-2.2.3 PixelFormatHdrEngine TESTS PASSED\n";
        return 0;
    }
    std::cerr << gFailureCount << " TEST(S) FAILED\n";
    return 1;
}
