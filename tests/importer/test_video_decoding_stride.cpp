#include "importer/stride_align.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

using openmedia::importer::StrideAlignmentEngine;

namespace {

void testOddWidthStride(uint32_t width, uint32_t bpp, uint32_t height) {
    const uint32_t computed = StrideAlignmentEngine::computeAlignedStride16(width, bpp);
    const uint32_t expected =
        static_cast<uint32_t>(std::ceil((static_cast<double>(width) * bpp) / 16.0) * 16.0);
    assert(computed == expected);
    assert((computed % 16U) == 0U);
    assert(computed >= width * bpp);
    assert(StrideAlignmentEngine::isValidAlignedStride(
        static_cast<int32_t>(computed), width, bpp));

    // Verify unaligned or undersized rowBytes are rejected
    assert(!StrideAlignmentEngine::isValidAlignedStride(
        static_cast<int32_t>(width * bpp), width, bpp) || ((width * bpp) % 16U == 0U));
    assert(!StrideAlignmentEngine::isValidAlignedStride(
        static_cast<int32_t>(computed - 4U), width, bpp));
    assert(!StrideAlignmentEngine::isValidAlignedStride(0, width, bpp));
    assert(!StrideAlignmentEngine::isValidAlignedStride(-16, width, bpp));

    // Canary-guarded destination buffer to detect any row/buffer overflow
    const uint32_t srcStride = width * bpp;
    std::vector<uint8_t> src(static_cast<size_t>(srcStride) * height, 0xA5);
    const size_t canaryBytes = 64;
    const uint8_t canaryVal = 0x5A;
    const size_t dstActiveBytes = static_cast<size_t>(computed) * height;
    std::vector<uint8_t> dst(dstActiveBytes + canaryBytes, canaryVal);

    const bool ok = StrideAlignmentEngine::copyWithRowBytes(
        src.data(),
        srcStride,
        dst.data(),
        static_cast<int32_t>(computed),
        width,
        height,
        bpp);
    assert(ok);

    // Verify active pixel bytes and zero-padded alignment tail per row
    for (uint32_t y = 0; y < height; ++y) {
        const size_t rowStart = static_cast<size_t>(y) * computed;
        for (uint32_t x = 0; x < srcStride; ++x) {
            assert(dst[rowStart + x] == 0xA5);
        }
        for (uint32_t p = srcStride; p < computed; ++p) {
            assert(dst[rowStart + p] == 0x00);
        }
    }

    // Verify 0 overflows into trailing canary guard
    for (size_t i = 0; i < canaryBytes; ++i) {
        assert(dst[dstActiveBytes + i] == canaryVal);
    }
}

} // namespace

int main() {
    // TC-2.2.4: Odd widths 1921, 721, 3839 with 4 BPP (BGRA_4444_8u) and 8 BPP (BGRA_4444_16u)
    testOddWidthStride(1921, 4, 8);
    testOddWidthStride(721, 4, 8);
    testOddWidthStride(3839, 4, 8);
    testOddWidthStride(1921, 8, 4);
    testOddWidthStride(721, 8, 4);
    testOddWidthStride(3839, 8, 4);

    // Edge cases
    assert(StrideAlignmentEngine::computeAlignedStride16(0, 4) == 0);
    assert(StrideAlignmentEngine::computeAlignedStride16(1921, 0) == 0);
    assert(StrideAlignmentEngine::computeAlignedStride16(0x40000000U, 4) == 0);

    std::cout << "TC-2.2.4 PASSED: All odd-width 16-byte stride alignment checks succeeded (0 overflows).\n";
    return 0;
}
